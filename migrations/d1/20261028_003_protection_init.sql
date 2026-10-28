-- mirrored from migrations/protection/20261028_003_init.sql by tools/d1-sync.js; edit the source, not this copy
-- Form protection: Cloudflare Turnstile keys (one row; the secret sealed
-- with CMS_SECRET), per-form switches for the registered forms, and daily
-- counts of what happened to submissions (passed, or why they were blocked).
CREATE TABLE IF NOT EXISTS protection_config (
	id             INTEGER PRIMARY KEY CHECK (id = 1),
	enabled        INTEGER NOT NULL DEFAULT 0,
	site_key       TEXT    NOT NULL DEFAULT '',
	secret         TEXT,
	appearance     TEXT    NOT NULL DEFAULT 'always',
	verified_at    INTEGER,
	updated_at     INTEGER NOT NULL,
	updated_by     INTEGER
);

CREATE TABLE IF NOT EXISTS protection_forms (
	form_id    TEXT    PRIMARY KEY,
	turnstile  TEXT    NOT NULL DEFAULT 'default',
	honeypot   INTEGER NOT NULL DEFAULT 1,
	updated_at INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS protection_stats (
	form_id TEXT    NOT NULL,
	day     TEXT    NOT NULL,
	outcome TEXT    NOT NULL,
	count   INTEGER NOT NULL DEFAULT 0,
	PRIMARY KEY (form_id, day, outcome)
);
