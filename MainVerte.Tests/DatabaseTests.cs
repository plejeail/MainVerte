using MainVerte.Core;

namespace MainVerteTests;


public class DatabaseTests
{
    private static string CreateTempDbPath() {
        string dir = Path.Combine(Path.GetTempPath(), "MainVerte.Tests", Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(dir);
        return Path.Combine(dir, "mv.db");
    }

    [Fact]
    public async Task Initialize_Creates_File_And_Allows_DDL_DML() {
        string    dbPath = CreateTempDbPath();
        using var db     = new Database();
        db.Initialize(dbPath);

        // Selon la plateforme et SQLite, le fichier peut être créé dès l'ouverture.
        // On ne fait donc pas d'assertion ici avant la première écriture.

        // DDL + DML
        var create = db.ExecuteNonQueryAsync("CREATE TABLE IF NOT EXISTS t(id INTEGER PRIMARY KEY, v INTEGER NOT NULL);");
        await create;

        var insert   = db.ExecuteNonQueryAsync("INSERT INTO t(v) VALUES (1);");
        int       inserted = await insert;
        Assert.Equal(1, inserted);

        // Après écriture, le fichier doit exister
        Assert.True(File.Exists(dbPath), "Le fichier de base doit exister après écriture");
    }

    [Fact]
    public async Task ExecuteScalarInt64Async_Returns_Correct_Count() {
        string    dbPath = CreateTempDbPath();
        using var db     = new Database();
        db.Initialize(dbPath);

        await db.ExecuteNonQueryAsync("CREATE TABLE t(id INTEGER PRIMARY KEY, v INTEGER NOT NULL);");

        for (int i = 0; i < 5; i++)
        {
            await db.ExecuteNonQueryAsync("INSERT INTO t(v) VALUES (42);");
        }

        long count = await db.ExecuteScalarInt64Async("SELECT COUNT(*) FROM t;");
        Assert.Equal(5L, count);
    }

    [Fact]
    public async Task Concurrent_Enqueues_Are_Serialized_By_DB_Thread() {
        string    dbPath = CreateTempDbPath();
        using var db     = new Database();
        db.Initialize(dbPath);

        await db.ExecuteNonQueryAsync("CREATE TABLE t(id INTEGER PRIMARY KEY, v INTEGER NOT NULL);");

        const int n     = 50;
        var    tasks = new Task[n];
        for (int i = 0; i < n; i++)
        {
            tasks[i] = db.ExecuteNonQueryAsync("INSERT INTO t(v) VALUES (7);");
        }

        await Task.WhenAll(tasks);

        long count = await db.ExecuteScalarInt64Async("SELECT COUNT(*) FROM t;");
        Assert.Equal(n, count);
    }

    [Fact]
    public async Task ListSpecimensAsync_Returns_Specimens_With_Species_And_Photo() {
        string    dbPath = CreateTempDbPath();
        using var db     = new Database();
        db.Initialize(dbPath);

        await db.ExecuteNonQueryAsync("""
            INSERT INTO gardener(id, display_name, created_at) VALUES (1, 'Test', 1000);
            INSERT INTO collection(id, gardener_id, name, created_at, modified_at)
                VALUES (10, 1, 'Collection', 1000, 1000);
            INSERT INTO species(id, common_name, created_at, modified_at)
                VALUES (7, 'Monstera deliciosa', 1000, 1000);
            INSERT INTO specimen(id, collection_id, species_id, display_name, photo_uri, created_at, modified_at)
                VALUES (3, 10, 7, 'Ma plante', 'photo://plant', 1000, 1000);
            INSERT INTO specimen(id, collection_id, display_name, created_at, modified_at)
                VALUES (4, 10, 'Sans espece', 1000, 1000);
            """);

        SpecimenSummary[] specimens = await db.ListSpecimensAsync();

        Assert.Equal(2, specimens.Length);
        Assert.Equal("Ma plante", specimens[0].Name);
        Assert.Equal("Monstera deliciosa", specimens[0].Species);
        Assert.Equal("photo://plant", specimens[0].PhotoUri);
        Assert.Equal("Sans espece", specimens[1].Name);
        Assert.Equal(String.Empty, specimens[1].Species);
        Assert.Null(specimens[1].PhotoUri);
    }

    [Fact]
    public async Task SpecimenCrudAsync_Creates_Reads_And_Updates_A_Specimen() {
        string    dbPath = CreateTempDbPath();
        using var db     = new Database();
        db.Initialize(dbPath);

        await db.ExecuteNonQueryAsync("""
            INSERT INTO gardener(id, display_name, created_at) VALUES (1, 'Test', 1000);
            INSERT INTO collection(id, gardener_id, name, created_at, modified_at)
                VALUES (11, 1, 'Collection', 1000, 1000);
            INSERT INTO species(id, common_name, created_at, modified_at)
                VALUES (7, 'Monstera deliciosa', 1000, 1000);
            """);

        var input = new SpecimenDetail(
            default,
            new MainVerteId(11),
            new MainVerteId(7),
            "Monstera deliciosa",
            null,
            "Ma plante",
            "photo://plant",
            1234L,
            CareRules.Empty,
            0,
            0);

        MainVerteId id = await db.CreateSpecimenAsync(input);
        SpecimenDetail? created = await db.GetSpecimenAsync(id);

        Assert.NotNull(created);
        Assert.Equal(11, created.CollectionId.Value);
        Assert.Equal(7, created.SpeciesId!.Value.Value);
        Assert.Equal("Monstera deliciosa", created.Species);
        Assert.Equal("Ma plante", created.DisplayName);
        Assert.Equal("photo://plant", created.PhotoUri);
        Assert.Equal(1234L, created.AcquiredAt);

        var updatedInput = input with {
            Id = id,
            DisplayName = "Ma nouvelle plante",
            SpeciesId = null,
            Species = null,
            PhotoUri = null,
            AcquiredAt = null,
        };
        Assert.True(await db.UpdateSpecimenAsync(updatedInput));

        SpecimenDetail? updated = await db.GetSpecimenAsync(id);
        Assert.NotNull(updated);
        Assert.Equal("Ma nouvelle plante", updated.DisplayName);
        Assert.Null(updated.SpeciesId);
        Assert.Null(updated.Species);
        Assert.Null(updated.PhotoUri);
        Assert.Null(updated.AcquiredAt);
        Assert.Null(await db.GetSpecimenAsync(new MainVerteId(999)));
        Assert.False(await db.UpdateSpecimenAsync(updatedInput with { Id = new MainVerteId(999) }));
    }

    [Fact]
    public async Task SpecimenEditorAsync_Creates_Updates_And_Deletes_A_Specimen() {
        string dbPath = CreateTempDbPath();
        using var db = new Database();
        db.Initialize(dbPath);

        await db.ExecuteNonQueryAsync("""
            INSERT INTO gardener(id, display_name, created_at) VALUES (1, 'Test', 1000);
            INSERT INTO collection(id, gardener_id, name, created_at, modified_at)
                VALUES (11, 1, 'Collection', 1000, 1000);
        """);

        SpecimenEditor editor = new(db);
        editor.StartNew(new MainVerteId(11));
        editor.UpdateDraft("Ma plante", "photo://plant");
        SpecimenDetail created = await editor.SaveAsync();

        Assert.NotEqual(MainVerteId.Invalid, created.Id);
        Assert.Equal("Ma plante", created.DisplayName);
        Assert.Equal("photo://plant", created.PhotoUri);

        editor.UpdateDraft("Ma nouvelle plante", null);
        SpecimenDetail updated = await editor.SaveAsync();

        Assert.Equal("Ma nouvelle plante", updated.DisplayName);
        Assert.Null(updated.PhotoUri);
        Assert.True(await editor.DeleteAsync());
        Assert.Null(await editor.LoadAsync(updated.Id));
    }

    [Fact]
    public async Task DeleteSpecimenAsync_Deletes_Existing_Specimen() {
        string dbPath = CreateTempDbPath();
        using var db = new Database();
        db.Initialize(dbPath);

        await db.ExecuteNonQueryAsync("""
            INSERT INTO gardener(id, display_name, created_at) VALUES (1, 'Test', 1000);
            INSERT INTO collection(id, gardener_id, name, created_at, modified_at)
                VALUES (11, 1, 'Collection', 1000, 1000);
            INSERT INTO specimen(id, collection_id, display_name, created_at, modified_at)
                VALUES (3, 11, 'Ma plante', 1000, 1000);
            """);

        MainVerteId specimenId = new(3);

        Assert.True(await db.DeleteSpecimenAsync(specimenId));
        Assert.Null(await db.GetSpecimenAsync(specimenId));
        Assert.Empty(await db.ListSpecimensAsync());
        Assert.False(await db.DeleteSpecimenAsync(specimenId));
    }

    [Fact]
    public async Task CareRuleCrudAsync_Persists_Rules_And_Finds_Due_Rules() {
        string dbPath = CreateTempDbPath();
        using var db = new Database();
        db.Initialize(dbPath);

        await db.ExecuteNonQueryAsync("""
            INSERT INTO gardener(id, display_name, created_at) VALUES (1, 'Test', 1000);
            INSERT INTO collection(id, gardener_id, name, created_at, modified_at)
                VALUES (11, 1, 'Collection', 1000, 1000);
            INSERT INTO specimen(id, collection_id, display_name, created_at, modified_at)
                VALUES (3, 11, 'Ma plante', 1000, 1000);
            """);

        DateTimeOffset dueDate = DateTimeOffset.FromUnixTimeSeconds(2000);
        CareRule rule = new() {
            SpecimenId = new MainVerteId(3),
            Type = CareType.WateringDate,
            CurrentValue = 10,
            ThresholdValue = 20,
            NextTrigger = dueDate,
            TriggerInterval = 86400,
        };

        MainVerteId ruleId = await db.AddCareRuleAsync(rule);
        rule.Id = ruleId;

        DateTimeOffset? nextTrigger = await db.GetNextTriggerDateAsync();
        Assert.Equal(dueDate, nextTrigger);

        CareRule[] dueRules = await db.GetRulesToProcessBeforeAsync(dueDate);
        Assert.Single(dueRules);
        Assert.Equal(ruleId, dueRules[0].Id);
        Assert.Equal(10, dueRules[0].CurrentValue);

        rule.NextTrigger = dueDate.AddDays(1);
        Assert.True(await db.UpdateCareRuleAsync(rule));
        Assert.Empty(await db.GetRulesToProcessBeforeAsync(dueDate));
        Assert.Equal(rule.NextTrigger, await db.GetNextTriggerDateAsync());

        Assert.True(await db.RemoveCareRuleAsync(rule));
        Assert.Null(await db.GetNextTriggerDateAsync());
        Assert.False(await db.RemoveCareRuleAsync(rule));
    }

    [Fact]
    public async Task SpecimenCrudAsync_Persists_CareRules() {
        string dbPath = CreateTempDbPath();
        using var db = new Database();
        db.Initialize(dbPath);

        await db.ExecuteNonQueryAsync("""
            INSERT INTO gardener(id, display_name, created_at) VALUES (1, 'Test', 1000);
            INSERT INTO collection(id, gardener_id, name, created_at, modified_at)
                VALUES (11, 1, 'Collection', 1000, 1000);
            """);

        CareRules rules = CareRules.Empty;
        rules[CareType.Fertilizing] = new CareRule {
            Type = CareType.Fertilizing,
            NextTrigger = DateTimeOffset.FromUnixTimeSeconds(3000),
            TriggerInterval = 604800,
        };

        MainVerteId id = await db.CreateSpecimenAsync(new SpecimenDetail(
            default,
            new MainVerteId(11),
            null,
            null,
            null,
            "Ma plante",
            null,
            null,
            rules,
            0,
            0));

        SpecimenDetail? specimen = await db.GetSpecimenAsync(id);
        Assert.NotNull(specimen);
        CareRule? rule = specimen.Rules[CareType.Fertilizing];
        Assert.NotNull(rule);
        Assert.Equal(id, rule.SpecimenId);
        Assert.Equal(DateTimeOffset.FromUnixTimeSeconds(3000), rule.NextTrigger);
    }

    [Fact]
    public async Task RescheduleCareRuleNowAsync_Uses_The_Current_Time_And_Interval() {
        string dbPath = CreateTempDbPath();
        using var db = new Database();
        db.Initialize(dbPath);

        await db.ExecuteNonQueryAsync("""
            INSERT INTO gardener(id, display_name, created_at) VALUES (1, 'Test', 1000);
            INSERT INTO collection(id, gardener_id, name, created_at, modified_at)
                VALUES (11, 1, 'Collection', 1000, 1000);
            INSERT INTO specimen(id, collection_id, display_name, created_at, modified_at)
                VALUES (3, 11, 'Ma plante', 1000, 1000);
            """);

        CareRule rule = new() {
            SpecimenId = new MainVerteId(3),
            Type = CareType.WateringDate,
            NextTrigger = DateTimeOffset.FromUnixTimeSeconds(2000),
            TriggerInterval = 86400,
        };
        rule.Id = await db.AddCareRuleAsync(rule);

        DateTimeOffset now = DateTimeOffset.FromUnixTimeSeconds(5000);
        DateTimeOffset? nextTrigger = await db.RescheduleCareRuleNowAsync(rule.Id, now);

        Assert.Equal(now.AddSeconds(rule.TriggerInterval), nextTrigger);
        CareRule[] dueRules = await db.GetRulesToProcessBeforeAsync(now);
        Assert.Empty(dueRules);
        Assert.Null(await db.RescheduleCareRuleNowAsync(new MainVerteId(999), now));
    }

    [Fact]
    public async Task Initialize_Applies_Embedded_Migration() {
        string    dbPath = CreateTempDbPath();
        using var db     = new Database();
        db.Initialize(dbPath);

        long version = await db.ExecuteScalarInt64Async("PRAGMA user_version;");
        const string query = "SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name = 'specimen';";
        long specimenTableCount = await db.ExecuteScalarInt64Async(query);

        Assert.Equal(2L, version);
        Assert.Equal(1L, specimenTableCount);
    }

    [Fact]
    public async Task Initialize_Creates_Default_Gardener_And_Collection() {
        string    dbPath = CreateTempDbPath();
        using var db     = new Database();
        db.Initialize(dbPath);

        long gardenerCount = await db.ExecuteScalarInt64Async("SELECT COUNT(*) FROM gardener WHERE id = 0;");
        long collectionCount = await db.ExecuteScalarInt64Async(
            "SELECT COUNT(*) FROM collection WHERE id = 0 AND gardener_id = 0;");

        Assert.Equal(1L, gardenerCount);
        Assert.Equal(1L, collectionCount);
    }
}
