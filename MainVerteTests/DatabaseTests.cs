namespace MainVerteTests;

using System;
using System.IO;
using System.Threading.Tasks;
using MainVerteCore;


public class DatabaseTests
{
    private static string CreateTempDbPath()
    {
        string dir = Path.Combine(Path.GetTempPath(), "MainVerte.Tests", Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(dir);
        return Path.Combine(dir, "mv.db");
    }

    [Fact]
    public void Initialize_Creates_File_And_Allows_DDL_DML()
    {
        string    dbPath = CreateTempDbPath();
        using var db     = new Database();
        db.Initialize(dbPath);

        // Selon la plateforme et SQLite, le fichier peut être créé dès l'ouverture.
        // On ne fait donc pas d'assertion ici avant la première écriture.

        // DDL + DML
        var create = db.ExecuteNonQueryAsync("CREATE TABLE IF NOT EXISTS t(id INTEGER PRIMARY KEY, v INTEGER NOT NULL);");
        create.GetAwaiter().GetResult();

        var insert   = db.ExecuteNonQueryAsync("INSERT INTO t(v) VALUES (1);");
        int       inserted = insert.GetAwaiter().GetResult();
        Assert.Equal(1, inserted);

        // Après écriture, le fichier doit exister
        Assert.True(File.Exists(dbPath), "Le fichier de base doit exister après écriture");
    }

    [Fact]
    public void ExecuteScalarInt64Async_Returns_Correct_Count()
    {
        string    dbPath = CreateTempDbPath();
        using var db     = new Database();
        db.Initialize(dbPath);

        db.ExecuteNonQueryAsync("CREATE TABLE t(id INTEGER PRIMARY KEY, v INTEGER NOT NULL);")
          .GetAwaiter().GetResult();

        for (int i = 0; i < 5; i++)
        {
            db.ExecuteNonQueryAsync("INSERT INTO t(v) VALUES (42);")
              .GetAwaiter().GetResult();
        }

        long count = db.ExecuteScalarInt64Async("SELECT COUNT(*) FROM t;")
                       .GetAwaiter().GetResult();
        Assert.Equal(5L, count);
    }

    [Fact]
    public void Concurrent_Enqueues_Are_Serialized_By_DB_Thread()
    {
        string    dbPath = CreateTempDbPath();
        using var db     = new Database();
        db.Initialize(dbPath);

        db.ExecuteNonQueryAsync("CREATE TABLE t(id INTEGER PRIMARY KEY, v INTEGER NOT NULL);")
          .GetAwaiter().GetResult();

        const int N     = 50;
        var    tasks = new Task[N];
        for (int i = 0; i < N; i++)
        {
            tasks[i] = db.ExecuteNonQueryAsync("INSERT INTO t(v) VALUES (7);");
        }

        Task.WhenAll(tasks).GetAwaiter().GetResult();

        long count = db.ExecuteScalarInt64Async("SELECT COUNT(*) FROM t;")
                       .GetAwaiter().GetResult();
        Assert.Equal(N, count);
    }
}
