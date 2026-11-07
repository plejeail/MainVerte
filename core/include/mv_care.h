#pragma once

#include <expected>
#include <vector>

#include "mv_definitions.h"
#include "mv_move_only_function.h"

namespace mv {

// Persisted values are stable across catalogue additions.
enum class NeedType { Watering = 0, Repotting = 1, Fertilizing = 2, Pruning = 3,
                      Treatment = 4, Light = 5, Humidity = 6, Temperature = 7 };

struct CareNeed {
    id_t id = 0;
    id_t specimen_id = 0;
    NeedType type = NeedType::Watering;
};

struct CareAction {
    enum class Kind { List, Add, Get, Delete };
    Kind kind = Kind::List;
    id_t specimen_id = 0;
    id_t id = 0;
    NeedType type = NeedType::Watering;
};
using CareResult = std::expected<std::vector<CareNeed>, MainverteError>;

// Owner-thread submission; requires initialized Services and a nonempty callback.
// Core owns inputs and results. Accepted jobs run on the existing SQLite worker.
// All outcomes (including queue rejection) are dispatched through platform callbacks.
// The return value reports queue acceptance only. Discarding delivery does not cancel writes.
// IDs are scoped to the current gardener captured at submission and specimen_id.
// List/Add/Delete return the committed specimen list; Get returns one need.
// Duplicate types are allowed. No events or scheduling state are created.
bool manage_care(CareAction action, MoveOnlyFunction<void(CareResult)> callback);

}  //  namespace mv
