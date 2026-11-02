-- 0000_init.sql


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

CREATE TABLE settings(
id INTEGER PRIMARY KEY CHECK (id = 1),
current_gardener_id   INTEGER REFERENCES gardener(id)   ON DELETE SET NULL,
current_collection_id INTEGER REFERENCES collection(id) ON DELETE SET NULL,
language              TEXT    NOT NULL DEFAULT '',
dark_theme            INTEGER NOT NULL DEFAULT 0 CHECK (dark_theme IN (0, 1)),
log_level             INTEGER NOT NULL DEFAULT 0 CHECK (log_level IN (0, 1, 2, 3)),
debug_enabled         INTEGER NOT NULL DEFAULT 0 CHECK (debug_enabled IN (0, 1))
);

INSERT INTO settings (id)
VALUES (1);

CREATE TABLE species(
id              INTEGER PRIMARY KEY,
common_name     TEXT NOT NULL,
scientific_name TEXT,
family_name     TEXT,
genus_name      TEXT,
photo_uri       TEXT,
created_at      INTEGER NOT NULL,
modified_at     INTEGER NOT NULL);

CREATE INDEX collection_gardener ON collection(gardener_id, id);
CREATE TABLE specimen(
    id INTEGER PRIMARY KEY,
    collection_id INTEGER NOT NULL REFERENCES collection(id) ON DELETE RESTRICT,
    display_name TEXT NOT NULL CHECK(length(trim(display_name)) > 0),
    botanical_name TEXT NOT NULL DEFAULT '',
    cultivar TEXT NOT NULL DEFAULT '',
    created_at INTEGER NOT NULL,
    modified_at INTEGER NOT NULL,
    archived_at INTEGER
);
CREATE INDEX specimen_collection ON specimen(collection_id, archived_at, id);
