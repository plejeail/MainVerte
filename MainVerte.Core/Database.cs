//! MainVerte.Core/Database.cs ------------------------------------------------
//!
//! DATABASE MANAGEMENT
//!
//! Everything database related: connection, configuration, migrations, queries.
//! ---------------------------------------------------------------------------
using System.Collections.Concurrent;
using System.Diagnostics;
using System.Reflection;
using Microsoft.Data.Sqlite;

namespace MainVerte.Core;


public sealed class Database : IDisposable
{
    private const long   DatabaseVersion = 3;
    private const string MigrationPrefix = "MainVerte.Core.Data.Migrations.";

    // Producer/consumer infrastructure (explicit, simple)
    private readonly ConcurrentQueue<IDatabaseJob> _queue        = new();
    private readonly SemaphoreSlim                 _pendingJobs = new(0);
    private          Thread?                       _dbThread;
    private readonly ManualResetEventSlim          _ready        = new(false);
    private readonly ManualResetEventSlim          _terminated   = new(false);
    private          string?                       _databasePath;
    private volatile bool                          _stopRequested;
    private          Exception?                    _initException;

    public void Initialize(string databasePath) {
        Require.NotEmpty(databasePath);
        if (_dbThread != null) {
            Log.Warn("database already initialized");
            return;
        }

        var stopwatch = Stopwatch.StartNew();
        if (!File.Exists(databasePath)) {
            string? databaseFolder = Path.GetDirectoryName(databasePath);
            if (!Directory.Exists(databaseFolder)) {
                Ensure.NotNull(databaseFolder);
                Directory.CreateDirectory(databaseFolder!);
                Log.Info("database folder created");
            }
        }

        _databasePath = databasePath;
        Log.Info("starting database thread");

        _dbThread = new Thread(DatabaseThreadStart) {
            IsBackground = true,
            Name = "MVDB",
        };
        _dbThread.Start();

        _ready.Wait();
        if (_initException != null) {
            // Re-throw the original initialization exception on the caller thread
            throw _initException;
        }
        Log.Info($"database initialisation done in {stopwatch.ElapsedMilliseconds}ms");
    }

    public void Dispose() {
        // Request termination and wake the database thread if sleeping
        _stopRequested = true;
        try { _pendingJobs.Release(); }
        catch {
            // ignored
        }

        if (_dbThread != null) {
            // Give the database thread a moment to drain and finish
            _terminated.Wait();
            _dbThread.Join();
            _dbThread = null;
        }
    }

    // ----------------------------
    // Database thread & pipeline
    // ----------------------------

    private interface IDatabaseJob {
        void Execute(SqliteConnection connection);
    }

    private sealed class DatabaseJob<TResult>(Func<SqliteConnection, TResult> job) : IDatabaseJob
    {
        private readonly Func<SqliteConnection, TResult> _job = job;
        private readonly TaskCompletionSource<TResult>   _tcs = new(TaskCreationOptions.RunContinuationsAsynchronously);

        public Task<TResult> Task() {
            return _tcs.Task;
        }

        public void Execute(SqliteConnection connection) {
            try {
                TResult result = _job(connection);
                _tcs.SetResult(result);
            } catch (Exception ex) {
                _tcs.SetException(ex);
            }
        }
    }

    private Task<TResult> Enqueue<TResult>(Func<SqliteConnection, TResult> job) {
        if (!_ready.IsSet) {
            throw new InvalidOperationException("Database not initialized");
        }

        DatabaseJob<TResult> wrapper = new(job);
        // Enqueue for the database thread to process
        _queue.Enqueue(wrapper);
        _pendingJobs.Release();
        return wrapper.Task();
    }

    private void DatabaseThreadStart() {
        Require.NotNull(_databasePath);

        SqliteConnection? connection = null;
        try {
            Log.Info("opening database connection");
            connection = new SqliteConnection($"Data Source={_databasePath}");
            connection.Open();

            // Configure and migrate before accepting jobs
            SetupDatabase(connection);
            MigrateDatabase(connection, ReadVersion(connection), DatabaseVersion);

            // Signal readiness
            _ready.Set();

            // Explicit job processing loop
            while (true) {
                _pendingJobs.Wait();
                if (_stopRequested) {
                    break;
                }

                while (_queue.TryDequeue(out IDatabaseJob? job)) {
                    job.Execute(connection);
                }
            }

            // Drain any remaining queued jobs before shutdown
            while (_queue.TryDequeue(out IDatabaseJob? remaining)) {
                remaining.Execute(connection);
            }
        } catch (Exception ex) {
            _initException = ex;
            try { _ready.Set(); }
            catch {
                // ignored
            }

            Log.Error($"database thread fatal error: {ex.Message}");
        } finally {
            try { connection?.Dispose(); }
            catch {
                // ignored
            }

            _terminated.Set();
        }
    }

    private static void SetupDatabase(SqliteConnection connection) {
        using SqliteCommand command = connection.CreateCommand();
        command.CommandText = """
            PRAGMA foreign_keys = ON;
            PRAGMA journal_mode = WAL;
            PRAGMA busy_timeout = 5000;
        """;
        command.ExecuteNonQuery();
    }

    private static long ReadVersion(SqliteConnection connection) {
        using SqliteCommand command = connection.CreateCommand();
        command.CommandText = "PRAGMA user_version;";
        return Convert.ToInt64(command.ExecuteScalar() ?? 0);
    }

    private static void WriteVersion(SqliteConnection connection, long version, SqliteTransaction transaction) {
        using SqliteCommand command = connection.CreateCommand();
        command.Transaction = transaction;
        command.CommandText = $"PRAGMA user_version = {version};";
        command.ExecuteNonQuery();
    }

    private static long ParseMigrationVersion(string resourceName) {
        string fileName = resourceName[MigrationPrefix.Length..];

        int underscoreIndex = fileName.IndexOf('_');
        Ensure.True(underscoreIndex > 0);

        string versionText = fileName[..underscoreIndex];
        bool versionIsInt = Int64.TryParse(versionText, out long version);
        Ensure.True(versionIsInt);

        return version;
    }

    private static bool IsMigrationResource(string resourceName) {
        return resourceName.StartsWith(MigrationPrefix, StringComparison.Ordinal)
            && resourceName.EndsWith(".sql", StringComparison.Ordinal);
    }

    private static void MigrateDatabase(SqliteConnection connection, long currentVersion, long softwareVersion) {
        if (softwareVersion < currentVersion) {
            throw new InvalidOperationException("Database version is newer than software version");
        }

        Assembly assembly       = typeof(Database).Assembly;
        string[] resourceNames = assembly.GetManifestResourceNames();
        Array.Sort(resourceNames, StringComparer.Ordinal);

        long migratedVersion = currentVersion;
        using SqliteTransaction transaction = connection.BeginTransaction();
        try {
            foreach (string resourceName in resourceNames) {
                if (!IsMigrationResource(resourceName)) {
                    continue;
                }

                long version = ParseMigrationVersion(resourceName);
                // Migration files are zero-based indexes. user_version stores
                // the schema version after the last applied migration.
                if (version < currentVersion || version >= softwareVersion) {
                    continue;
                }

                Ensure.True(version == migratedVersion);
                migratedVersion = version + 1;

                Log.Info($"migrating database from version {version} to {migratedVersion}");

                ExecuteScript(connection, resourceName, transaction);
            }

            WriteVersion(connection, migratedVersion, transaction);
            Ensure.True(migratedVersion == softwareVersion);

            transaction.Commit();
        } catch {
            transaction.Rollback();
            throw;
        }
    }

    private static string LoadEmbeddedSql(string resourceName) {
        Assembly assembly = typeof(Database).Assembly;

        using Stream? stream = assembly.GetManifestResourceStream(resourceName);
        Ensure.True(stream != null);

        using var reader = new StreamReader(stream!);
        return reader.ReadToEnd();
    }

    private static void ExecuteScript(SqliteConnection connection, string resourceName, SqliteTransaction transaction)
    {
        string script = LoadEmbeddedSql(resourceName);
        using SqliteCommand command = connection.CreateCommand();
        command.Transaction = transaction;
        command.CommandText = script;
        command.ExecuteNonQuery();
    }

    internal Task<int> ExecuteNonQueryAsync(string sql)
    {
        Require.NotEmpty(sql);
        return Enqueue(connection => {
            using SqliteCommand cmd = connection.CreateCommand();
            cmd.CommandText = sql;
            return cmd.ExecuteNonQuery();
        });
    }

    internal Task<long> ExecuteScalarInt64Async(string sql)
    {
        Require.NotEmpty(sql);
        return Enqueue(connection => {
            using SqliteCommand cmd = connection.CreateCommand();
            cmd.CommandText = sql;
            object? value = cmd.ExecuteScalar();
            return Convert.ToInt64(value ?? 0);
        });
    }

    public Task<SpecimenSummary[]> ListSpecimensAsync() {
        return Enqueue(connection => {
            using SqliteCommand cmd = connection.CreateCommand();
            cmd.CommandText = """
                SELECT s.id,
                       s.display_name,
                       COALESCE(sp.common_name, ''),
                       s.photo_uri
                FROM specimen AS s
                LEFT JOIN species AS sp ON sp.id = s.species_id
                ORDER BY s.id;
            """;

            using SqliteDataReader reader = cmd.ExecuteReader();
            var specimens = new List<SpecimenSummary>();
            while (reader.Read()) {
                string? photoUri = reader.IsDBNull(3) ? null : reader.GetString(3);
                specimens.Add(new SpecimenSummary(
                    new MainVerteId(reader.GetInt64(0)),
                    reader.GetString(1),
                    reader.GetString(2),
                    photoUri));
            }

            return specimens.ToArray();
        });
    }

    public Task<SpecimenDetail?> GetSpecimenAsync(MainVerteId id) {
        return Enqueue(connection => {
            using SqliteCommand cmd = connection.CreateCommand();
            cmd.CommandText = """
                SELECT s.id,
                       s.collection_id,
                       s.species_id,
                       sp.common_name,
                       s.location_id,
                       s.display_name,
                       s.photo_uri,
                       s.acquired_at,
                       s.created_at,
                       s.modified_at
                FROM specimen AS s
                LEFT JOIN species AS sp ON sp.id = s.species_id
                WHERE s.id = $id;
            """;
            cmd.Parameters.AddWithValue("$id", id.Value);

            using SqliteDataReader reader = cmd.ExecuteReader();
            if (!reader.Read()) {
                return null;
            }

            SpecimenDetail specimen = ReadSpecimenDetail(reader);
            return specimen with { Rules = ReadCareRules(connection, specimen.Id) };
        });
    }

    public Task<MainVerteId> CreateSpecimenAsync(SpecimenDetail specimen) {
        ValidateSpecimen(specimen);

        return Enqueue(connection => {
            long now = DateTimeOffset.UtcNow.ToUnixTimeSeconds();
            using SqliteTransaction transaction = connection.BeginTransaction();
            using SqliteCommand cmd = connection.CreateCommand();
            cmd.Transaction = transaction;
            cmd.CommandText = """
                INSERT INTO specimen (
                    collection_id,
                    species_id,
                    location_id,
                    display_name,
                    photo_uri,
                    acquired_at,
                    created_at,
                    modified_at
                )
                VALUES (
                    $collection_id,
                    $species_id,
                    $location_id,
                    $display_name,
                    $photo_uri,
                    $acquired_at,
                    $created_at,
                    $modified_at
                )
                RETURNING id;
            """;
            AddSpecimenParameters(cmd, specimen);
            cmd.Parameters.AddWithValue("$created_at", now);
            cmd.Parameters.AddWithValue("$modified_at", now);

            MainVerteId id = new(Convert.ToInt64(cmd.ExecuteScalar()));
            WriteCareRules(connection, transaction, id, specimen.Rules);
            transaction.Commit();
            return id;
        });
    }

    public Task<bool> UpdateSpecimenAsync(SpecimenDetail specimen) {
        Require.NotNull(specimen);
        ValidateSpecimen(specimen);

        return Enqueue(connection => {
            using SqliteTransaction transaction = connection.BeginTransaction();
            using SqliteCommand cmd = connection.CreateCommand();
            cmd.Transaction = transaction;
            cmd.CommandText = """
                UPDATE specimen
                SET species_id = $species_id,
                    location_id = $location_id,
                    display_name = $display_name,
                    photo_uri = $photo_uri,
                    acquired_at = $acquired_at,
                    modified_at = $modified_at
                WHERE id = $id;
            """;
            AddSpecimenParameters(cmd, specimen);
            cmd.Parameters.AddWithValue("$id", specimen.Id.Value);
            cmd.Parameters.AddWithValue(
                "$modified_at",
                DateTimeOffset.UtcNow.ToUnixTimeSeconds());

            bool updated = cmd.ExecuteNonQuery() == 1;
            if (updated) {
                DeleteCareRules(connection, transaction, specimen.Id);
                WriteCareRules(connection, transaction, specimen.Id, specimen.Rules);
            }

            transaction.Commit();
            return updated;
        });
    }

    [Conditional("DEBUG")]
    private static void ValidateSpecimen(SpecimenDetail specimen) {
        Require.NotNull(specimen);
        Require.True(!String.IsNullOrWhiteSpace(specimen.DisplayName));
    }

    private static void AddSpecimenParameters(SqliteCommand cmd, SpecimenDetail specimen) {
        cmd.Parameters.AddWithValue("$collection_id", specimen.CollectionId.Value);
        cmd.Parameters.AddWithValue(
            "$species_id",
            specimen.SpeciesId?.Value ?? (object)DBNull.Value);
        cmd.Parameters.AddWithValue(
            "$location_id",
            specimen.LocationId?.Value ?? (object)DBNull.Value);
        cmd.Parameters.AddWithValue("$display_name", specimen.DisplayName);
        cmd.Parameters.AddWithValue(
            "$photo_uri",
            specimen.PhotoUri is null ? DBNull.Value : specimen.PhotoUri);
        cmd.Parameters.AddWithValue(
            "$acquired_at",
            specimen.AcquiredAt ?? (object)DBNull.Value);
    }

    private static CareRules ReadCareRules(SqliteConnection connection, MainVerteId specimenId) {
        var rules = CareRules.Empty;
        using SqliteCommand cmd = connection.CreateCommand();
        cmd.CommandText = """
            SELECT id, specimen_id, type, current_value, threshold_value, next_trigger, trigger_interval
            FROM care_rule
            WHERE specimen_id = $specimen_id
            ORDER BY type;
            """;
        cmd.Parameters.AddWithValue("$specimen_id", specimenId.Value);

        using SqliteDataReader reader = cmd.ExecuteReader();
        while (reader.Read()) {
            CareRule rule = ReadCareRule(reader);
            ValidateCareRule(rule);
            rules[rule.Type] = rule;
        }

        return rules;
    }

    private static CareRule ReadCareRule(SqliteDataReader reader) {
        return new CareRule {
            Id = new MainVerteId(reader.GetInt64(0)),
            SpecimenId = new MainVerteId(reader.GetInt64(1)),
            Type = (CareType)reader.GetInt32(2),
            CurrentValue = reader.GetInt64(3),
            ThresholdValue = reader.GetInt64(4),
            NextTrigger = DateTimeOffset.FromUnixTimeSeconds(reader.GetInt64(5)),
            TriggerInterval = reader.GetInt32(6),
        };
    }

    private static void WriteCareRules(SqliteConnection connection,
                                       SqliteTransaction transaction,
                                       MainVerteId specimenId,
                                       CareRules rules) {
        for (int index = 0; index < (int)CareType.Count; index++) {
            var type = (CareType)index;
            CareRule? rule = rules[type];
            if (rule == null) {
                continue;
            }

            ValidateCareRule(rule, false);
            using SqliteCommand cmd = connection.CreateCommand();
            cmd.Transaction = transaction;
            cmd.CommandText = """
                INSERT INTO care_rule (
                    specimen_id,
                    type,
                    current_value,
                    threshold_value,
                    next_trigger,
                    trigger_interval
                )
                VALUES (
                    $specimen_id,
                    $type,
                    $current_value,
                    $threshold_value,
                    $next_trigger,
                    $range
                );
                """;
            cmd.Parameters.AddWithValue("$specimen_id", specimenId.Value);
            cmd.Parameters.AddWithValue("$type", (int)type);
            cmd.Parameters.AddWithValue("$current_value", rule.CurrentValue);
            cmd.Parameters.AddWithValue("$threshold_value", rule.ThresholdValue);
            cmd.Parameters.AddWithValue("$next_trigger", rule.NextTrigger.ToUnixTimeSeconds());
            cmd.Parameters.AddWithValue("$range", rule.TriggerInterval);
            cmd.ExecuteNonQuery();
        }
    }

    private static void DeleteCareRules(SqliteConnection connection, SqliteTransaction transaction, MainVerteId specimenId) {
        using SqliteCommand cmd = connection.CreateCommand();
        cmd.Transaction = transaction;
        cmd.CommandText = "DELETE FROM care_rule WHERE specimen_id = $specimen_id;";
        cmd.Parameters.AddWithValue("$specimen_id", specimenId.Value);
        cmd.ExecuteNonQuery();
    }

    [Conditional("DEBUG")]
    private static void ValidateCareRule(CareRule rule, bool withSpecimenId = true) {
        Require.NotNull(rule);
        if (withSpecimenId) {
            Require.True(rule.SpecimenId != MainVerteId.Invalid);
        }
        Require.IsInRange(rule.Type);
        Require.True(rule.TriggerInterval > 0);
    }

    public Task<MainVerteId> AddCareRuleAsync(CareRule rule) {
        Require.NotNull(rule);
        ValidateCareRule(rule);

        return Enqueue(connection => {
            using SqliteCommand cmd = connection.CreateCommand();
            cmd.CommandText = """
                INSERT INTO care_rule (
                    specimen_id, type, current_value, threshold_value, next_trigger, trigger_interval
                )
                VALUES ($specimen_id, $type, $current_value, $threshold_value, $next_trigger, $range)
                RETURNING id;
                """;
            AddCareRuleParameters(cmd, rule);
            return new MainVerteId(Convert.ToInt64(cmd.ExecuteScalar()));
        });
    }

    public Task<bool> RemoveCareRuleAsync(CareRule rule) {
        Require.NotNull(rule);
        return Enqueue(connection => {
            using SqliteCommand cmd = connection.CreateCommand();
            cmd.CommandText = "DELETE FROM care_rule WHERE id = $id;";
            cmd.Parameters.AddWithValue("$id", rule.Id.Value);
            return cmd.ExecuteNonQuery() == 1;
        });
    }

    public Task<bool> UpdateCareRuleAsync(CareRule rule) {
        ValidateCareRule(rule);

        return Enqueue(connection => {
            using SqliteCommand cmd = connection.CreateCommand();
            cmd.CommandText = """
                UPDATE care_rule
                SET
                    specimen_id = $specimen_id,
                    type = $type,
                    current_value = $current_value,
                    threshold_value = $threshold_value,
                    next_trigger = $next_trigger,
                    trigger_interval = $range
                WHERE id = $id;
                """;
            AddCareRuleParameters(cmd, rule);
            cmd.Parameters.AddWithValue("$id", rule.Id.Value);
            return cmd.ExecuteNonQuery() == 1;
        });
    }

    public Task<DateTimeOffset?> RescheduleCareRuleNowAsync(MainVerteId ruleId, DateTimeOffset now) {
        return Enqueue(connection => {
            using SqliteCommand cmd = connection.CreateCommand();
            cmd.CommandText = """
                UPDATE care_rule
                SET next_trigger = $now + trigger_interval
                WHERE id = $id
                RETURNING next_trigger;
                """;
            cmd.Parameters.AddWithValue("$now", now.ToUnixTimeSeconds());
            cmd.Parameters.AddWithValue("$id", ruleId.Value);

            object? value = cmd.ExecuteScalar();
            if (value == null || value == DBNull.Value) {
                return (DateTimeOffset?)null;
            }

            return DateTimeOffset.FromUnixTimeSeconds(Convert.ToInt64(value));
        });
    }

    public Task<DateTimeOffset?> GetNextTriggerDateAsync() {
        return Enqueue(connection => {
            using SqliteCommand cmd = connection.CreateCommand();
            cmd.CommandText = "SELECT MIN(next_trigger) FROM care_rule;";
            object? value = cmd.ExecuteScalar();
            if (value == null || value == DBNull.Value) {
                return (DateTimeOffset?)null;
            }

            return DateTimeOffset.FromUnixTimeSeconds(Convert.ToInt64(value));
        });
    }

    public Task<CareRule[]> GetRulesToProcessBeforeAsync(DateTimeOffset date) {
        long timestamp = date.ToUnixTimeSeconds();
        return Enqueue(connection => {
            using SqliteCommand cmd = connection.CreateCommand();
            cmd.CommandText = """
                SELECT id, specimen_id, type, current_value, threshold_value, next_trigger, trigger_interval
                FROM care_rule
                WHERE next_trigger <= $next_trigger
                ORDER BY next_trigger, id;
                """;
            cmd.Parameters.AddWithValue("$next_trigger", timestamp);

            using SqliteDataReader reader = cmd.ExecuteReader();
            var rules = new List<CareRule>();
            while (reader.Read()) {
                CareRule rule = ReadCareRule(reader);
                ValidateCareRule(rule);
                rules.Add(rule);
            }

            return rules.ToArray();
        });
    }

    private static void AddCareRuleParameters(SqliteCommand cmd, CareRule rule) {
        cmd.Parameters.AddWithValue("$specimen_id", rule.SpecimenId.Value);
        cmd.Parameters.AddWithValue("$type", (int)rule.Type);
        cmd.Parameters.AddWithValue("$current_value", rule.CurrentValue);
        cmd.Parameters.AddWithValue("$threshold_value", rule.ThresholdValue);
        cmd.Parameters.AddWithValue("$next_trigger", rule.NextTrigger.ToUnixTimeSeconds());
        cmd.Parameters.AddWithValue("$range", rule.TriggerInterval);
    }

    private static SpecimenDetail ReadSpecimenDetail(SqliteDataReader reader) {
        MainVerteId? speciesId = reader.IsDBNull(2)
            ? null
            : new MainVerteId(reader.GetInt64(2));
        string? species = reader.IsDBNull(3) ? null : reader.GetString(3);
        MainVerteId? locationId = reader.IsDBNull(4)
            ? null
            : new MainVerteId(reader.GetInt64(4));
        string? photoUri = reader.IsDBNull(6) ? null : reader.GetString(6);
        long? acquiredAt = reader.IsDBNull(7) ? null : reader.GetInt64(7);

        return new SpecimenDetail(
            new MainVerteId(reader.GetInt64(0)),
            new MainVerteId(reader.GetInt64(1)),
            speciesId,
            species,
            locationId,
            reader.GetString(5),
            photoUri,
            acquiredAt,
            CareRules.Empty,
            reader.GetInt64(8),
            reader.GetInt64(9));
    }

    public Task<bool> DeleteSpecimenAsync(MainVerteId id) {
        return Enqueue(connection => {
            using SqliteCommand cmd = connection.CreateCommand();
            cmd.CommandText = """
                DELETE FROM specimen
                WHERE id = $id;
            """;
            cmd.Parameters.AddWithValue("$id", id.Value);

            return cmd.ExecuteNonQuery() == 1;
        });
    }
}
