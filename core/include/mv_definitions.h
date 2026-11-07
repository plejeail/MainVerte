// include/mv_definitions.h ---------------------------------------------------
// MAINVERTE_CORE DOMAIN DEFINITIONS
//
// Domain values and explicit operation results shared with the application.
//
//-------------------------------------------------------------- mainverte/core
#pragma once

#include <cstdint>
#include <expected>
#include <string>


namespace mv {

/// Database object identifier; persisted objects have positive IDs.
/// Settings use zero to represent an absent selection.
using id_t = int64_t;

/// Operation failure: inspect code for handling and message for diagnostics.
struct MainverteError {
    enum class Code : uint8_t {
        NotRunning,  ///< The database worker refused the submission.
        Database,    ///< A database operation failed after submission.
        InvalidInput,
        NotFound,
        CollectionNotEmpty,
        LastCollection
    };

    Code code;
    /// Owned diagnostic text; not a stable identifier or a localized UI message.
    std::string message;
};

/// Owned gardener data; independent of database and callback lifetimes.
struct Gardener {
    id_t id;
    std::string name;
};

/// Completed operation outcome, distinct from whether its job was accepted.
using GardenerResult = std::expected<Gardener, MainverteError>;

enum class NeedType : int8_t {
    Watering    = 0,
    Repotting   = 1,
    Fertilizing = 2,
    Pruning     = 3,
    Treatment   = 4,
    Light       = 5,
    Humidity    = 6,
    Temperature = 7,
};

struct CareNeed {
    id_t id = 0;
    id_t specimen_id = 0;
    NeedType type = NeedType::Watering;
};

struct CareAction {
    enum class Kind : int8_t { List, Add, Get, Delete };
    Kind kind = Kind::List;
    id_t specimen_id = 0;
    id_t id = 0;
    NeedType type = NeedType::Watering;
};

using CareResult = std::expected<std::vector<CareNeed>, MainverteError>;

// Descriptive metadata only; profiles do not create or configure care needs.
// Persisted values are stable across catalogue additions.
enum class PlantProfile : int8_t {
    Succulent = 0,
    Tropical  = 1,
    FruitTree = 2,
    Herb      = 3,
    Other     = 4,
};

struct Collection {
    id_t id = 0;
    std::string name = {};
    int64_t created_at = 0;
    int64_t modified_at = 0;
};

struct Specimen {
    id_t id = 0;
    id_t collection_id = 0;
    std::string display_name = {};
    std::string botanical_name = {};
    std::string cultivar = {};
    bool archived = false;
    int64_t created_at = 0;
    int64_t modified_at = 0;
    std::optional<int64_t> archived_at = {};
    std::optional<PlantProfile> profile = {};
    // Calendar date (YYYY-MM-DD), not an insertion timestamp or biological age.
    std::optional<std::string> acquired_on = {};
};

struct Plants {
    std::vector<Collection> collections;
    std::vector<Specimen> specimens;
    id_t selected = 0;
};

using PlantsResult = std::expected<Plants, MainverteError>;

// Owner-thread submissions. All inputs/results are owned; callbacks are dispatched
// by the platform, including rejection. A true return means queue acceptance only.
// Operations always scope IDs to the current gardener captured at submission.
struct PlantAction {
    enum class Kind { Browse, CreateCollection, RenameCollection, SelectCollection,
                      DeleteCollection, CreateSpecimen, UpdateSpecimen, MoveSpecimen,
                      ArchiveSpecimen, RestoreSpecimen, GetSpecimen, ListSpecimens };
    Kind kind = Kind::Browse;
    id_t id = 0;
    id_t destination = 0;
    std::string name = {};
    std::string botanical_name = {};
    std::string cultivar = {};
    std::optional<PlantProfile> profile = {};
    std::optional<std::string> acquired_on = {};
    // UpdateSpecimen preserves omitted optional metadata by default. Set this to
    // replace both fields, including explicitly clearing them with nullopt.
    // Non-null values always update the corresponding field. Ignored on creation.
    bool replace_optional_metadata = false;
};

}  // namespace mv
