namespace MainVerte.Core;


public sealed class SpecimenEditor
{
    private readonly Database _database;
    private SpecimenDetail? _originalSpecimen;
    private SpecimenDetail? _draftSpecimen;

    public SpecimenDetail? Specimen => _draftSpecimen;

    public bool IsNew { get; private set; }

    public SpecimenEditor(Database database) {
        Require.NotNull(database);
        _database = database;
    }

    public async Task<SpecimenDetail?> LoadAsync(MainVerteId specimenId) {
        SpecimenDetail? specimen = await _database.GetSpecimenAsync(specimenId);
        _originalSpecimen = specimen;
        _draftSpecimen = specimen;
        IsNew = false;
        return specimen;
    }

    public void StartNew(MainVerteId collectionId) {
        if (collectionId == MainVerteId.Invalid) {
            throw new InvalidOperationException("Missing collection identifier for specimen creation.");
        }

        _originalSpecimen = null;
        _draftSpecimen = new SpecimenDetail(MainVerteId.Invalid,
                                            collectionId,
                                            null,
                                            null,
                                            null,
                                            String.Empty,
                                            null,
                                            null,
                                            CareRules.Empty,
                                            0,
                                            0);
        IsNew = true;
    }

    public void UpdateDraft(string displayName, string? photoUri) {
        SpecimenDetail? draft = _draftSpecimen;
        if (draft == null) {
            throw new InvalidOperationException("No specimen is being edited.");
        }

        _draftSpecimen = draft with {
            DisplayName = displayName,
            PhotoUri = photoUri,
        };
    }

    public void SetCareRule(CareType type, CareRule? rule) {
        SpecimenDetail? draft = _draftSpecimen;
        if (draft == null) {
            throw new InvalidOperationException("No specimen is being edited.");
        }

        CareRules rules = draft.Rules;
        rules[type] = rule;
        _draftSpecimen = draft with { Rules = rules };
    }

    public async Task<DateTimeOffset?> RescheduleCareRuleNowAsync(CareType type, DateTimeOffset now) {
        SpecimenDetail? draft = _draftSpecimen;
        if (draft == null) {
            throw new InvalidOperationException("No specimen is loaded.");
        }

        CareRule? rule = draft.Rules[type];
        if (rule == null) {
            return null;
        }

        DateTimeOffset? nextTrigger = await _database.RescheduleCareRuleNowAsync(rule.Id, now);
        if (!nextTrigger.HasValue) {
            return null;
        }

        CareRule updatedRule = new() {
            Id = rule.Id,
            SpecimenId = rule.SpecimenId,
            Type = rule.Type,
            TriggerInterval = rule.TriggerInterval,
            CurrentValue = rule.CurrentValue,
            ThresholdValue = rule.ThresholdValue,
            NextTrigger = nextTrigger.Value,
        };
        CareRules rules = draft.Rules;
        rules[type] = updatedRule;
        _draftSpecimen = draft with { Rules = rules };
        _originalSpecimen = _draftSpecimen;
        return nextTrigger;
    }

    public async Task<SpecimenDetail> SaveAsync() {
        Require.NotNull(_draftSpecimen);
        SpecimenDetail? draft = _draftSpecimen;

        if (draft == null) {
            throw new InvalidOperationException("No specimen is being saved.");
        }

        SpecimenDetail savedSpecimen;
        if (IsNew) {
            MainVerteId specimenId = await _database.CreateSpecimenAsync(draft);
            savedSpecimen = draft with { Id = specimenId };
        } else {
            bool updated = await _database.UpdateSpecimenAsync(draft);
            if (!updated) {
                throw new InvalidOperationException("Specimen no longer exists.");
            }

            savedSpecimen = draft;
        }

        _originalSpecimen = savedSpecimen;
        _draftSpecimen = savedSpecimen;
        IsNew = false;
        return savedSpecimen;
    }

    public void Cancel() {
        if (IsNew) {
            _originalSpecimen = null;
            _draftSpecimen = null;
            IsNew = false;
            return;
        }

        _draftSpecimen = _originalSpecimen;
    }

    public Task<bool> DeleteAsync() {
        SpecimenDetail? specimen = _draftSpecimen;
        if (specimen == null) {
            return Task.FromResult(false);
        }

        return _database.DeleteSpecimenAsync(specimen.Id);
    }
}
