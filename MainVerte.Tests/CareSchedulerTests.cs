using MainVerte.Core;

namespace MainVerteTests;


public class CareSchedulerTests
{
    [Fact]
    public async Task InitializeAsync_Plans_The_Earliest_CareRule() {
        string dbPath = CreateTempDbPath();
        using var db = CreateDatabaseWithSpecimen(dbPath);
        DateTimeOffset firstTrigger = DateTimeOffset.FromUnixTimeSeconds(2000);
        DateTimeOffset laterTrigger = firstTrigger.AddHours(1);
        await db.AddCareRuleAsync(CreateRule(CareType.WateringDate, firstTrigger, 3600));
        await db.AddCareRuleAsync(CreateRule(CareType.Fertilizing, laterTrigger, 3600));

        var platform = new SchedulerTestPlatform();
        Platform.SetImplementation(platform);
        try {
            var scheduler = new CareScheduler(db);
            await scheduler.InitializeAsync();

            Assert.Equal(firstTrigger, platform.ScheduledTrigger);
        } finally {
            Platform.SetImplementation(new SchedulerTestPlatform());
        }
    }

    [Fact]
    public async Task ProcessCareAsync_Publishes_Event_And_Advances_Overdue_Rule() {
        string dbPath = CreateTempDbPath();
        using var db = CreateDatabaseWithSpecimen(dbPath);
        DateTimeOffset trigger = DateTimeOffset.FromUnixTimeSeconds(1000);
        CareRule rule = CreateRule(CareType.WateringDate, trigger, 100);
        rule.CurrentValue = 12;
        rule.ThresholdValue = 24;
        rule.Id = await db.AddCareRuleAsync(rule);

        var platform = new SchedulerTestPlatform();
        Platform.SetImplementation(platform);
        try {
            var scheduler = new CareScheduler(db);
            await scheduler.ProcessCareAsync(DateTimeOffset.FromUnixTimeSeconds(1350));

            Assert.Single(platform.PublishedEvents);
            var careEvent = Assert.IsType<CareEvent>(platform.PublishedEvents[0]);
            Assert.Equal(rule.Id, careEvent.RuleId);
            Assert.Equal(rule.SpecimenId, careEvent.SpecimenId);
            Assert.Equal(rule.Type, careEvent.Type);
            Assert.Equal(rule.CurrentValue, careEvent.CurrentValue);
            Assert.Equal(rule.ThresholdValue, careEvent.ThresholdValue);
            Assert.Equal(trigger, careEvent.TriggeredAt);

            CareRule[] dueRules = await db.GetRulesToProcessBeforeAsync(DateTimeOffset.FromUnixTimeSeconds(1350));
            Assert.Empty(dueRules);
            Assert.Equal(DateTimeOffset.FromUnixTimeSeconds(1400), await db.GetNextTriggerDateAsync());
            Assert.Equal(DateTimeOffset.FromUnixTimeSeconds(1400), platform.ScheduledTrigger);
        } finally {
            Platform.SetImplementation(new SchedulerTestPlatform());
        }
    }

    private static CareRule CreateRule(CareType type, DateTimeOffset nextTrigger, int interval) {
        return new CareRule {
            SpecimenId = new MainVerteId(3),
            Type = type,
            NextTrigger = nextTrigger,
            TriggerInterval = interval,
        };
    }

    private static Database CreateDatabaseWithSpecimen(string dbPath) {
        var db = new Database();
        db.Initialize(dbPath);
        db.ExecuteNonQueryAsync("""
            INSERT INTO gardener(id, display_name, created_at) VALUES (1, 'Test', 1000);
            INSERT INTO collection(id, gardener_id, name, created_at, modified_at)
                VALUES (11, 1, 'Collection', 1000, 1000);
            INSERT INTO specimen(id, collection_id, display_name, created_at, modified_at)
                VALUES (3, 11, 'Ma plante', 1000, 1000);
            """).GetAwaiter().GetResult();
        return db;
    }

    private static string CreateTempDbPath() {
        string dir = Path.Combine(Path.GetTempPath(), "MainVerte.Tests", Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(dir);
        return Path.Combine(dir, "mv.db");
    }

    private sealed class SchedulerTestPlatform : IPlatform
    {
        public readonly List<MainVerteEvent> PublishedEvents = new();
        public DateTimeOffset? ScheduledTrigger { get; private set; }

        public void LogMessage(string message, LogLevel level) {}

        public string ApplicationPath() {
            return String.Empty;
        }

        public void UserFeedback(string message, FeedbackKind kind) {}

        public void Publish(MainVerteEvent payload) {
            PublishedEvents.Add(payload);
        }

        public void UpdateSchedulerTriggerTime(DateTimeOffset newDate) {
            ScheduledTrigger = newDate;
        }
    }
}
