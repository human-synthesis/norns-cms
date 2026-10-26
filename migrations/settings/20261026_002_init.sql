CREATE TABLE IF NOT EXISTS settings (
	"group"    TEXT    PRIMARY KEY,
	value      TEXT    NOT NULL DEFAULT '{}',
	updated_at INTEGER NOT NULL,
	updated_by INTEGER
);
