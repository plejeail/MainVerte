#pragma once

#include <expected>
#include <optional>
#include <string>
#include <vector>

#include "mv_definitions.h"
#include "mv_move_only_function.h"

namespace mv {
// Descriptive metadata only; profiles do not create or configure care needs.
// Persisted values are stable across catalogue additions.
enum class PlantProfile { Succulent = 0, Tropical = 1, FruitTree = 2, Herb = 3, Other = 4 };

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
// Browse returns collections plus all active/archived specimens of the selected
// collection. GetSpecimen returns one owned specimen by id (across collections).
// ListSpecimens uses id as a collection ID without changing persisted selection.
// Every successful mutation returns a fresh selected-collection snapshot.
// No operation permanently deletes specimens.
bool manage_plants(PlantAction action, std::string initial_collection_name,
                   MoveOnlyFunction<void(PlantsResult)> callback);
}  //  namespace mv
