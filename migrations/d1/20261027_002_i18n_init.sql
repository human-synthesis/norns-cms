-- mirrored from migrations/i18n/20261027_002_init.sql by tools/d1-sync.js; edit the source, not this copy
-- Languages and localisations (markets). Seeded so the existing site keeps
-- its URLs: one primary localisation on the main host, English, no prefix.
CREATE TABLE IF NOT EXISTS languages (
	code       TEXT    PRIMARY KEY,               -- BCP 47: en, de, de-CH, pt-BR
	name       TEXT    NOT NULL,                  -- English name
	native     TEXT    NOT NULL DEFAULT '',       -- name in the language itself
	dir        TEXT    NOT NULL DEFAULT 'ltr',    -- ltr | rtl
	fallback   TEXT    NOT NULL DEFAULT '',       -- language to fall back to (de-CH → de)
	paths      TEXT    NOT NULL DEFAULT '{}',     -- URL words: { "article": "artikel", "authors": "autoren", "category": "kategorie", "tag": "schlagwort" }
	enabled    INTEGER NOT NULL DEFAULT 1,
	sort       INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE IF NOT EXISTS localisations (
	id               INTEGER PRIMARY KEY AUTOINCREMENT,
	code             TEXT    NOT NULL UNIQUE,      -- e.g. ch, de, global
	name             TEXT    NOT NULL,
	mode             TEXT    NOT NULL DEFAULT 'path', -- path | subdomain | domain
	prefix           TEXT    NOT NULL DEFAULT '',     -- path mode: first URL segment ('' = none)
	host             TEXT    NOT NULL DEFAULT '',     -- subdomain / domain mode: full host name
	languages        TEXT    NOT NULL DEFAULT '[]',   -- [{ "code": "de", "segment": "de" }]
	default_language TEXT    NOT NULL,
	prefix_default   INTEGER NOT NULL DEFAULT 0,      -- 1: the default language also gets its segment
	countries        TEXT    NOT NULL DEFAULT '[]',   -- ISO codes: hreflang regions, visitor suggestions
	content_fallback INTEGER NOT NULL DEFAULT 0,      -- show the fallback language's entry when a translation is missing
	is_primary       INTEGER NOT NULL DEFAULT 0,
	enabled          INTEGER NOT NULL DEFAULT 1,
	sort             INTEGER NOT NULL DEFAULT 0,
	created_at       INTEGER NOT NULL
);

-- Overrides of the built-in interface texts (i18n/shared/catalog.js).
CREATE TABLE IF NOT EXISTS strings (
	locale     TEXT    NOT NULL,
	key        TEXT    NOT NULL,
	value      TEXT    NOT NULL,
	updated_at INTEGER NOT NULL,
	PRIMARY KEY (locale, key)
);

INSERT OR IGNORE INTO languages (code, name, native, dir, sort) VALUES ('en', 'English', 'English', 'ltr', 0);
INSERT OR IGNORE INTO localisations (code, name, mode, languages, default_language, is_primary, created_at)
	VALUES ('default', 'Default', 'path', '[{"code":"en","segment":"en"}]', 'en', 1, 0);
