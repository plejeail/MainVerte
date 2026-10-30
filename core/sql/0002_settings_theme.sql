ALTER TABLE settings ADD COLUMN dark_theme INTEGER NOT NULL DEFAULT 0 CHECK (dark_theme IN (0, 1));
