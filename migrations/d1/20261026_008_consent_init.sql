-- mirrored from migrations/consent/20261026_008_init.sql by tools/d1-sync.js; edit the source, not this copy
CREATE TABLE IF NOT EXISTS consent_scripts (
	id         INTEGER PRIMARY KEY AUTOINCREMENT,
	name       TEXT    NOT NULL,
	provider   TEXT    NOT NULL,                  -- see consent/server/providers.js
	category   TEXT    NOT NULL,                  -- preferences | analytics | marketing
	placement  TEXT    NOT NULL DEFAULT 'head',   -- head | body
	config     TEXT    NOT NULL DEFAULT '{}',
	code       TEXT    NOT NULL DEFAULT '',       -- custom provider only
	cookies    TEXT    NOT NULL DEFAULT '',       -- one per line: name | duration | purpose (custom provider)
	enabled    INTEGER NOT NULL DEFAULT 1,
	sort       INTEGER NOT NULL DEFAULT 0,
	updated_at INTEGER NOT NULL,
	updated_by INTEGER
);

CREATE TABLE IF NOT EXISTS consent_records (
	id             INTEGER PRIMARY KEY AUTOINCREMENT,
	consent_id     TEXT    NOT NULL,
	user_id        INTEGER,
	policy_version INTEGER NOT NULL,
	action         TEXT    NOT NULL,              -- accept_all | reject_all | custom | withdraw
	categories     TEXT    NOT NULL,              -- JSON { preferences, analytics, marketing }
	country        TEXT    NOT NULL DEFAULT '',
	region_mode    TEXT    NOT NULL DEFAULT '',
	gpc            INTEGER NOT NULL DEFAULT 0,
	banner_hash    TEXT    NOT NULL DEFAULT '',
	created_at     INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS consent_records_cid_idx ON consent_records(consent_id, created_at);
CREATE INDEX IF NOT EXISTS consent_records_created_idx ON consent_records(created_at);
