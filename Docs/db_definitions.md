# Database Definitions

## Payloads

Payloads are stored as BLOB. They should be encoded as utf8 JSON.

## Dates

Dates are stored as integer counting seconds since the Unix epoch. All date variables have a '_at' suffix. 

## MVP bootstrap

Migration `0001_bootstrap_default_collection.sql` creates the default gardener and default collection with `id = 0`. MVP specimen creation uses collection `0` until collection selection is implemented.

## Enumerations

### Event.Category
| value | enum     |
|-------|----------|
| 0     | none     |
| 1001  | system   |
| 1002  | gardener |
| 1003  | device   |

### Event.Actor_type
| value | enum     |
|-------|----------|
| 0     | none     |
| 1001  | system   |
| 1002  | gardener |
| 1003  | device   |
| 1004  | import   |

## Species.Climate
| value | enum            |
|-------|-----------------|
| 0     | none            |
| 1001  | equatorial      |
| 1002  | tropical        |
| 1003  | subtropical     |
| 1004  | warm_temperate  |
| 1005  | cool_temperate  |
| 1006  | mediterranean   |
| 1007  | boreal          |
| 1008  | polar           | 

## Species.Biome
| value | enum                       |
|-------|----------------------------|
| 0     | none                       |
| 1001  | tropical_rainforest        |
| 1002  | tropical_seasonal_forest   |
| 1003  | cloud_forest               |
| 1004  | temperate_forest           |
| 1005  | mediterranean_shrubland    |
| 1006  | savanna                    |
| 1007  | grassland                  |
| 1008  | desert                     |
| 1009  | semi_desert                |
| 1010  | alpine                     |
| 1011  | wetland                    |
| 1012  | riparian                   |

## Species.Hydric_regime
| value | enum    |
|-------|---------|
| 0     | none    |
| 1001  | arid    |
| 1002  | dry     |
| 1003  | mesic   |
| 1004  | moist   |
| 1005  | wet     |
| 1006  | aquatic |

## Species.Hydric_seasonality
| value | enum              |
|-------|-------------------|
| 0     | none              |
| 1001  | stable            |
| 1002  | summer_dry        |
| 1003  | winter_dry        |
| 1004  | seasonal_dormancy |
| 1005  | seasonal_flooding |
| 1006  | irregula          |

# Species.Lifetime
| value | enum                  |
|-------|-----------------------|
| 0     | none                  |
| 1001  | annual                |
| 1002  | biennial              |
| 1003  | short_lived_perennial |
| 1004  | perennial             |

# CareRule.Source
| value | enum   |
|-------|--------|
| 0     | none   |
| 1001  | user   |
| 1002  | system |

# CareRule.Type
| value | enum                |
|-------|---------------------|
| 0     | none                |
| 1001  | custom              |
| 1002  | general_inspection  |
| 1003  | watering            |
| 1004  | fertilizing         |
| 1005  | repotting           |
| 1006  | pot_rotation        |
| 1007  | pruning             |
| 1008  | pinching            |
| 1009  | treatment           |
| 1010  | winter_protection   |
| 1011  | humidity_adjustment |
| 1012  | light_adjustment    |
| 1013  | measurement         |

# CareRule.Severity
| value | enum     |
|-------|----------|
| 0     | none     |
| 1001  | low      |
| 1002  | warning  |
| 1003  | critical |

## CareRule storage

`CareRule` records are stored in the `care_rule` table. The scheduling fields are stored as typed SQLite integers:

| column | meaning |
|--------|---------|
| `specimen_id` | specimen receiving the care rule |
| `type` | care action type |
| `current_value` | current measured or accumulated value |
| `threshold_value` | threshold used when the rule is value-based |
| `next_trigger` | next trigger as Unix epoch seconds |
| `range` | recurrence interval in seconds |
