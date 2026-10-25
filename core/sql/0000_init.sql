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
current_collection_id INTEGER REFERENCES collection(id) ON DELETE SET NULL
);

INSERT INTO settings (id)
VALUES (1);
