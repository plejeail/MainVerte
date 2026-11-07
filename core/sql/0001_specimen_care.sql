ALTER TABLE specimen ADD COLUMN profile INTEGER CHECK(profile IN (0,1,2,3,4));
ALTER TABLE specimen ADD COLUMN acquired_on TEXT;

-- Identity is independent of type. Repeated types are deliberately permitted.
CREATE TABLE care_need(
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    specimen_id INTEGER NOT NULL REFERENCES specimen(id) ON DELETE CASCADE,
    type INTEGER NOT NULL CHECK(type IN (0,1,2,3,4,5,6,7))
);
CREATE INDEX care_need_specimen ON care_need(specimen_id, id);

-- Future historical events must not depend on a cascading reference to care_need.
-- Removing a need deletes its configuration, not interventions that occurred.
