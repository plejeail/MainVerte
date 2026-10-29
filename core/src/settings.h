#pragma once

#include "definitions.h"


struct Settings {
    mv::id_t current_gardener_id{0};
    mv::id_t current_collection_id{0};
};

Settings read_all_settings();
void save_settings();
