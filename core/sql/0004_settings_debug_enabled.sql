ALTER TABLE settings ADD COLUMN debug_enabled INTEGER NOT NULL DEFAULT 0 CHECK (debug_enabled IN (0, 1));
