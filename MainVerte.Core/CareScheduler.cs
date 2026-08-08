namespace MainVerte.Core;


public class CareScheduler(Database database)
{
    private DateTimeOffset _nextTriggerDate;
    private readonly Database _database = database;

    public async Task InitializeAsync() {
        _nextTriggerDate = DateTimeOffset.MaxValue;
        await ReplanNextTriggerAsync();
    }

    public async Task AddCareRuleAsync(CareRule rule) {
        Require.NotNull(rule);
        MainVerteId id = await _database.AddCareRuleAsync(rule);
        rule.Id = id;

        if (rule.NextTrigger < _nextTriggerDate) {
            await ReplanNextTriggerAsync();
        }
    }

    public async Task<bool> RemoveCareRuleAsync(CareRule rule) {
        Require.NotNull(rule);
        bool removed = await _database.RemoveCareRuleAsync(rule);
        if (removed && rule.NextTrigger <= _nextTriggerDate) {
            await ReplanNextTriggerAsync();
        }

        return removed;
    }

    public async Task<bool> UpdateCareRuleAsync(CareRule rule) {
        Require.NotNull(rule);
        bool updated = await _database.UpdateCareRuleAsync(rule);
        if (updated) {
            await ReplanNextTriggerAsync();
        }

        return updated;
    }

    public async Task ReplanNextTriggerAsync() {
        DateTimeOffset? nextTriggerDate = await _database.GetNextTriggerDateAsync();
        if (nextTriggerDate.HasValue) {
            _nextTriggerDate = nextTriggerDate.Value;
            Platform.UpdateSchedulerTriggerTime(_nextTriggerDate);
        }
    }

    public async Task ProcessCareAsync(DateTimeOffset now) {
        CareRule[] rules = await _database.GetRulesToProcessBeforeAsync(now);
        foreach (CareRule rule in rules) {
            DateTimeOffset triggeredAt = rule.NextTrigger;
            if (IsTriggered(rule)) {
                Platform.Publish(CreateCareEvent(rule, triggeredAt));
            }

            do {
                rule.NextTrigger = rule.NextTrigger.AddSeconds(rule.TriggerInterval);
            } while (rule.NextTrigger <= now);

            await _database.UpdateCareRuleAsync(rule);
        }

        await ReplanNextTriggerAsync();
    }

    private static bool IsTriggered(CareRule rule) {
        switch (rule.Type) {
        case CareType.WateringDate:
        case CareType.Repotting:
        case CareType.Fertilizing:
        case CareType.TurningPot:
            return true;
        case CareType.Count:
        default:
            Log.Error($"Rule {rule.Type} is invalid.");
            return false;
        }
    }

    private static CareEvent CreateCareEvent(CareRule rule, DateTimeOffset triggeredAt) {
        return new CareEvent {
            RuleId = rule.Id,
            SpecimenId = rule.SpecimenId,
            Type = rule.Type,
            CurrentValue = rule.CurrentValue,
            ThresholdValue = rule.ThresholdValue,
            TriggeredAt = triggeredAt,
        };
    }
}
