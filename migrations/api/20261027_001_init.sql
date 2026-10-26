-- API keys for the management API (/api/v1). Only a SHA-256 of the key is
-- stored; the key itself is shown once when created. A key acts for the
-- staff user who created it, limited to the capabilities listed here AND
-- still held by that user at request time.
CREATE TABLE IF NOT EXISTS api_keys (
	id            INTEGER PRIMARY KEY AUTOINCREMENT,
	name          TEXT    NOT NULL,
	prefix        TEXT    NOT NULL,                -- first characters, shown to tell keys apart
	hash          TEXT    NOT NULL UNIQUE,
	user_id       INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
	capabilities  TEXT    NOT NULL DEFAULT '[]',
	expires_at    INTEGER,
	last_used_at  INTEGER,
	revoked_at    INTEGER,
	created_at    INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS api_keys_user_idx ON api_keys(user_id);
