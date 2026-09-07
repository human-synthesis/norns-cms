-- mirrored from migrations/content/20260907_001_init.sql by tools/d1-sync.js; edit the source, not this copy
CREATE TABLE IF NOT EXISTS entries (
	id           INTEGER PRIMARY KEY AUTOINCREMENT,
	type         TEXT    NOT NULL,
	slug         TEXT    NOT NULL,
	status       TEXT    NOT NULL DEFAULT 'draft',
	title        TEXT    NOT NULL DEFAULT '',
	data         TEXT    NOT NULL DEFAULT '{}',
	author_id    INTEGER,
	published_at INTEGER,
	created_at   INTEGER NOT NULL,
	updated_at   INTEGER NOT NULL,
	UNIQUE (type, slug)
);

CREATE INDEX IF NOT EXISTS entries_type_status_idx ON entries(type, status, updated_at);

CREATE TABLE IF NOT EXISTS revisions (
	id         INTEGER PRIMARY KEY AUTOINCREMENT,
	entry_id   INTEGER NOT NULL REFERENCES entries(id) ON DELETE CASCADE,
	data       TEXT    NOT NULL,
	author_id  INTEGER,
	created_at INTEGER NOT NULL
);

CREATE INDEX IF NOT EXISTS revisions_entry_idx ON revisions(entry_id, created_at);
