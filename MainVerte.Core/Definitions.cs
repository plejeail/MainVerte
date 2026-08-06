//! MainVerte.Core/Definitions.cs ---------------------------------------------
//! DOMAIN TYPES
//!
//! Those types shall not have intelligence, they are just data containers.
//! --------------------------------------------------------------------------
namespace MainVerte.Core;

public readonly record struct MainVerteId(int Value) {
    public static readonly MainVerteId Invalid = new(-1);
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
    long CreatedAt,
    long ModifiedAt);

public sealed record SpeciesSummary(MainVerteId Id, string Name);
public sealed record SpeciesDetail(SpeciesSummary Summary);
