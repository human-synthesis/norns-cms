-- mirrored from migrations/auth/20260907_001_init.sql by tools/d1-sync.js; edit the source, not this copy
CREATE TABLE IF NOT EXISTS users (
	id            INTEGER PRIMARY KEY AUTOINCREMENT,
	email         TEXT    NOT NULL UNIQUE,
	name          TEXT    NOT NULL DEFAULT '',
	password_hash TEXT    NOT NULL,
	role          TEXT    NOT NULL DEFAULT 'editor',
	created_at    INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS sessions (
	id         TEXT    PRIMARY KEY,
	user_id    INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
	expires_at INTEGER NOT NULL,
	created_at INTEGER NOT NULL
);

CREATE INDEX IF NOT EXISTS sessions_user_idx ON sessions(user_id);
