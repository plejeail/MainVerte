DROP TABLE IF EXISTS rule;

CREATE TABLE IF NOT EXISTS care_rule(
id               INTEGER PRIMARY KEY,
specimen_id      INTEGER NOT NULL REFERENCES specimen(id) ON DELETE CASCADE,
type             INTEGER NOT NULL,
current_value    INTEGER NOT NULL,
threshold_value  INTEGER NOT NULL,
next_trigger     INTEGER NOT NULL,
trigger_interval INTEGER NOT NULL);

CREATE INDEX IF NOT EXISTS care_rule_next_trigger_index
ON care_rule(next_trigger, id);

CREATE INDEX IF NOT EXISTS care_rule_specimen_index
ON care_rule(specimen_id, type);
