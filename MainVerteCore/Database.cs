using System.Collections.Concurrent;
using System.Diagnostics;
using System.Reflection;

namespace MainVerteCore;

using System;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.Data.Sqlite;

public sealed class Database : IDisposable
{
    private const long   DatabaseVersion = 1;
    private const string MigrationPrefix = "MainVerte.Data.Migrations.";

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

    private Task<TResult> Enqueue<TResult>(Func<SqliteConnection, TResult> job)
    {
        if (!_ready.IsSet) {
            throw new InvalidOperationException("Database not initialized");
        }

        DatabaseJob<TResult> wrapper = new(job);
        // Enqueue for the database thread to process
        _queue.Enqueue(wrapper);
        _pendingJobs.Release();
        return wrapper.Task();
    }

    private void DatabaseThreadStart()
    {
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
        if (underscoreIndex <= 0) {
            throw new InvalidOperationException($"Invalid migration name '{resourceName}'.");
        }


        string versionText = fileName[..underscoreIndex];
        if (!Int64.TryParse(versionText, out long version)) {
            throw new InvalidOperationException($"Invalid migration version in '{resourceName}'.");
        }

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

        using SqliteTransaction transaction = connection.BeginTransaction();
        try {
            foreach (string resourceName in resourceNames) {
                if (!IsMigrationResource(resourceName)) {
                    continue;
                }

                long version = ParseMigrationVersion(resourceName);
                if (version <= currentVersion || version > softwareVersion) {
                    continue;
                }

                Log.Info($"migrating database from version {version - 1} to {version}");

                ExecuteScript(connection, resourceName, transaction);
                WriteVersion(connection, version, transaction);
            }

            transaction.Commit();
        } catch {
            transaction.Rollback();
            throw;
        }
    }

    private static string LoadEmbeddedSql(string resourceName) {
        Assembly assembly = typeof(Database).Assembly;

        using Stream? stream = assembly.GetManifestResourceStream(resourceName);
        if (stream is null) {
            throw new InvalidOperationException($"Resource '{resourceName}' not found.");
        }

        using var reader = new StreamReader(stream);
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

    // ----------------------------
    // Example wrappers (usage pattern)
    // ----------------------------
    // These show how public APIs should enqueue work. They are placeholders
    // for future strongly typed operations and can be expanded as needed.

    public Task<int> ExecuteNonQueryAsync(string sql)
    {
        Require.NotEmpty(sql);
        return Enqueue(connection => {
            using SqliteCommand cmd = connection.CreateCommand();
            cmd.CommandText = sql;
            return cmd.ExecuteNonQuery();
        });
    }

    public Task<long> ExecuteScalarInt64Async(string sql)
    {
        Require.NotEmpty(sql);
        return Enqueue(connection => {
            using SqliteCommand cmd = connection.CreateCommand();
            cmd.CommandText = sql;
            object? value = cmd.ExecuteScalar();
            return Convert.ToInt64(value ?? 0);
        });
    }
}
