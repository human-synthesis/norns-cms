-- mirrored from migrations/settings/20261026_002_init.sql by tools/d1-sync.js; edit the source, not this copy
CREATE TABLE IF NOT EXISTS settings (
	"group"    TEXT    PRIMARY KEY,
	value      TEXT    NOT NULL DEFAULT '{}',
	updated_at INTEGER NOT NULL,
	updated_by INTEGER
);
