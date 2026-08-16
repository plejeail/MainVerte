//! MainVerte.Core/Definitions.cs ---------------------------------------------
//! DOMAIN TYPES
//!
//! Those types shall not have intelligence, they are just data containers.
//! --------------------------------------------------------------------------

using System.Runtime.CompilerServices;

namespace MainVerte.Core;

public abstract class MainVerteEvent;

public readonly record struct MainVerteId(long Value) {
    public static readonly MainVerteId Invalid = new(-1);
}

public enum CareType
{
    WateringDate    = 0,
    Repotting   = 1,
    Fertilizing = 2,
    Rotation = 3,
    Count,
}

public class CareRule
{
    public MainVerteId Id = MainVerteId.Invalid;
    public MainVerteId SpecimenId = MainVerteId.Invalid;
    public CareType Type;
    public int TriggerInterval;
    public long CurrentValue;
    public long ThresholdValue;
    public DateTimeOffset NextTrigger;
}

public struct CareRules
{
    [InlineArray((int)CareType.Count)]
    private struct CareRulesBuffer
    {
        private CareRule? _element0;
    }

    private CareRulesBuffer _values;

    public CareRule? this[CareType type]
    {
        readonly get {
            Require.IsInRange(type);
            return _values[(int)type];
        }

        set {
            Require.IsInRange(type);
            _values[(int)type] = value;
        }
    }

    public static CareRules Empty => default;
}

public class CareEvent : MainVerteEvent
{
    public MainVerteId RuleId;
    public MainVerteId SpecimenId;
    public string      SpecimenName = String.Empty;
    public CareType    Type;
    public long        CurrentValue;
    public long        ThresholdValue;
    public DateTimeOffset TriggeredAt;
}

public sealed record SpecimenSummary(MainVerteId Id,
                                     string Name,
                                     string Species,
                                     string? PhotoUri);

public sealed record SpecimenDetail(
    MainVerteId Id,
    MainVerteId CollectionId,
    MainVerteId? SpeciesId,
    string? Species,
    MainVerteId? LocationId,
    string DisplayName,
    string? PhotoUri,
    long? AcquiredAt,
    CareRules Rules,
    long CreatedAt,
    long ModifiedAt);

public sealed record SpeciesSummary(MainVerteId Id, string Name);

public sealed record SpeciesDetail(SpeciesSummary Summary);
