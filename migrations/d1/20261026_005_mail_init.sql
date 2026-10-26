-- mirrored from migrations/mail/20261026_005_init.sql by tools/d1-sync.js; edit the source, not this copy
CREATE TABLE IF NOT EXISTS mail_log (
	id         INTEGER PRIMARY KEY AUTOINCREMENT,
	recipient  TEXT    NOT NULL,
	subject    TEXT    NOT NULL,
	template   TEXT    NOT NULL DEFAULT '',
	provider   TEXT    NOT NULL,                  -- resend | log
	status     TEXT    NOT NULL,                  -- sent | failed | logged
	body       TEXT    NOT NULL DEFAULT '',       -- only kept for the log provider (dev); never with a real provider
	error      TEXT    NOT NULL DEFAULT '',
	created_at INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS mail_log_created_idx ON mail_log(created_at);
