CREATE TABLE IF NOT EXISTS redirects (
	id         INTEGER PRIMARY KEY AUTOINCREMENT,
	from_path  TEXT    NOT NULL UNIQUE,
	to_path    TEXT    NOT NULL DEFAULT '',
	status     INTEGER NOT NULL DEFAULT 301,      -- 301 | 302 | 410
	source     TEXT    NOT NULL DEFAULT 'manual', -- manual | auto | import
	hits       INTEGER NOT NULL DEFAULT 0,
	last_hit_at INTEGER,
	created_at INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS not_found (
	path       TEXT    PRIMARY KEY,
	hits       INTEGER NOT NULL DEFAULT 0,
	referrer   TEXT    NOT NULL DEFAULT '',       -- host only
	first_seen INTEGER NOT NULL,
	last_seen  INTEGER NOT NULL
);

INSERT OR IGNORE INTO redirects (from_path, to_path, status, source, created_at) VALUES ('/posts', '/articles', 301, 'auto', 0);
