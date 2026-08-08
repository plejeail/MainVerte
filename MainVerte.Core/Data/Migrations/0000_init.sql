CREATE TABLE gardener(
id           INTEGER PRIMARY KEY,
display_name TEXT    NOT NULL,
created_at   INTEGER NOT NULL);

CREATE TABLE collection(
id           INTEGER PRIMARY KEY,
gardener_id  INTEGER NOT NULL REFERENCES gardener(id) ON DELETE CASCADE,
name         TEXT    NOT NULL,
created_at   INTEGER NOT NULL,
modified_at  INTEGER NOT NULL);

CREATE TABLE species(
id            INTEGER PRIMARY KEY,
common_name   TEXT    NOT NULL,
scientific_name TEXT,
family_name   TEXT,
genus_name    TEXT,
photo_uri     TEXT,
climate       INTEGER,
biome         INTEGER,
hydric_regime INTEGER,
hydric_seasonality INTEGER,
lifetime      INTEGER,
created_at    INTEGER NOT NULL,
modified_at   INTEGER NOT NULL);

CREATE TABLE location(
id          INTEGER PRIMARY KEY,
gardener_id INTEGER NOT NULL REFERENCES gardener(id) ON DELETE CASCADE,
name        TEXT    NOT NULL,
created_at  INTEGER NOT NULL);

CREATE TABLE specimen(
id            INTEGER PRIMARY KEY,
collection_id INTEGER NOT NULL REFERENCES collection(id) ON DELETE CASCADE,
species_id    INTEGER REFERENCES species(id)  ON DELETE SET NULL,
location_id   INTEGER REFERENCES location(id) ON DELETE SET NULL,
display_name  TEXT    NOT NULL,
photo_uri     TEXT,
acquired_at   INTEGER,
created_at    INTEGER NOT NULL,
modified_at   INTEGER NOT NULL);

CREATE TABLE journal(
id          INTEGER PRIMARY KEY,
specimen_id INTEGER REFERENCES specimen(id) ON DELETE CASCADE,
name        TEXT    NOT NULL,
is_system   INTEGER NOT NULL CHECK(is_system IN(0, 1)),
created_at  INTEGER NOT NULL,
modified_at INTEGER NOT NULL);

CREATE TABLE event(
id         INTEGER PRIMARY KEY,
journal_id INTEGER NOT NULL REFERENCES journal(id) ON DELETE CASCADE,
created_at INTEGER NOT NULL,
category   INTEGER NOT NULL,
type       INTEGER NOT NULL,
actor_type INTEGER NOT NULL,
payload    BLOB    NOT NULL);

CREATE TABLE care_rule(
id               INTEGER PRIMARY KEY,
specimen_id      INTEGER NOT NULL REFERENCES specimen(id) ON DELETE CASCADE,
type             INTEGER NOT NULL,
current_value    INTEGER NOT NULL,
threshold_value  INTEGER NOT NULL,
next_trigger     INTEGER NOT NULL,
trigger_interval INTEGER NOT NULL);

CREATE TABLE alert(
id           INTEGER PRIMARY KEY,
gardener_id  INTEGER REFERENCES gardener(id) ON DELETE CASCADE,
specimen_id  INTEGER REFERENCES specimen(id) ON DELETE CASCADE,
message      TEXT    NOT NULL,
rule_type    INTEGER,
triggered_at INTEGER NOT NULL,
severity     INTEGER NOT NULL,
created_at   INTEGER NOT NULL);
