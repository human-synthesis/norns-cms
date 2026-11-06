-- Search engine notifications: URLs waiting to be sent through IndexNow,
-- a log of every submission (IndexNow and Google Search Console) and small
-- state values (the IndexNow key, when Google last got the sitemap).

CREATE TABLE IF NOT EXISTS indexing_queue (
	url       TEXT    PRIMARY KEY,
	queued_at INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS indexing_log (
	id         INTEGER PRIMARY KEY AUTOINCREMENT,
	engine     TEXT    NOT NULL,                  -- indexnow | google
	target     TEXT    NOT NULL DEFAULT '',       -- host, or the sitemap URL
	urls       INTEGER NOT NULL DEFAULT 0,
	ok         INTEGER NOT NULL DEFAULT 0,
	status     INTEGER,
	message    TEXT    NOT NULL DEFAULT '',
	created_at INTEGER NOT NULL
);

CREATE INDEX IF NOT EXISTS indexing_log_created ON indexing_log (created_at);

CREATE TABLE IF NOT EXISTS seo_state (
	key   TEXT PRIMARY KEY,
	value TEXT NOT NULL
);
