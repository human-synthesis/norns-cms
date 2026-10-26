CREATE TABLE IF NOT EXISTS media (
	id         INTEGER PRIMARY KEY AUTOINCREMENT,
	key        TEXT    NOT NULL UNIQUE,           -- storage key, also the public path /media/<key>
	filename   TEXT    NOT NULL,
	mime       TEXT    NOT NULL,
	size       INTEGER NOT NULL,
	width      INTEGER,
	height     INTEGER,
	alt        TEXT    NOT NULL DEFAULT '',
	caption    TEXT    NOT NULL DEFAULT '',
	author_id  INTEGER,
	created_at INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS media_created_idx ON media(created_at);

CREATE TABLE IF NOT EXISTS media_usage (
	media_id INTEGER NOT NULL REFERENCES media(id) ON DELETE CASCADE,
	entry_id INTEGER NOT NULL REFERENCES entries(id) ON DELETE CASCADE,
	PRIMARY KEY (media_id, entry_id)
);
CREATE INDEX IF NOT EXISTS media_usage_entry_idx ON media_usage(entry_id);
