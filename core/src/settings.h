#pragma once


struct Settings {
    int32_t current_gardener_id{0};
    int32_t current_collection_id{0};
};

Settings read_all_settings();
void save_settings();
