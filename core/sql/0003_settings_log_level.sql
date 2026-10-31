ALTER TABLE settings ADD COLUMN log_level INTEGER NOT NULL DEFAULT 0 CHECK (log_level IN (0, 1, 2, 3));
