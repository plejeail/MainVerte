INSERT OR IGNORE INTO gardener (id, display_name, created_at)
VALUES (0, 'Moi', CAST(strftime('%s', 'now') AS INTEGER));

INSERT OR IGNORE INTO collection (id, gardener_id, name, created_at, modified_at)
VALUES (0, 0, 'Ma collection', CAST(strftime('%s', 'now') AS INTEGER), CAST(strftime('%s', 'now') AS INTEGER));
