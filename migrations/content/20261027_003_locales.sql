-- Per-language entries. SQLite cannot drop the UNIQUE(type, slug) table
-- constraint, so `entries` is rebuilt with UNIQUE(type, locale, slug).
-- Dropping the old table cascades into revisions, media_usage and
-- entry_terms, so their rows are set aside first and restored after (ids
-- are kept, so every reference still points at the same entry).
CREATE TABLE _bk_revisions AS SELECT * FROM revisions;
CREATE TABLE _bk_media_usage AS SELECT * FROM media_usage;
CREATE TABLE _bk_entry_terms AS SELECT * FROM entry_terms;

CREATE TABLE entries_v3 (
	id                INTEGER PRIMARY KEY AUTOINCREMENT,
	type              TEXT    NOT NULL,
	locale            TEXT    NOT NULL DEFAULT 'en',
	slug              TEXT    NOT NULL,
	status            TEXT    NOT NULL DEFAULT 'draft',
	visibility        TEXT    NOT NULL DEFAULT 'public',
	title             TEXT    NOT NULL DEFAULT '',
	data              TEXT    NOT NULL DEFAULT '{}',
	rendered          TEXT    NOT NULL DEFAULT '{}',
	markets           TEXT    NOT NULL DEFAULT '[]',  -- localisation codes; [] = everywhere the language is offered
	translation_group INTEGER,                        -- id of the group (the first entry's id)
	translation_of    INTEGER,                        -- the entry this one was translated from
	source_updated_at INTEGER,                        -- the source's updated_at when translated
	author_id         INTEGER,
	published_at      INTEGER,
	created_at        INTEGER NOT NULL,
	updated_at        INTEGER NOT NULL,
	UNIQUE (type, locale, slug)
);

INSERT INTO entries_v3 (id, type, locale, slug, status, visibility, title, data, rendered, markets, translation_group, author_id, published_at, created_at, updated_at)
	SELECT id, type, 'en', slug, status, visibility, title, data, rendered, '[]', id, author_id, published_at, created_at, updated_at FROM entries;

DROP TABLE entries;
ALTER TABLE entries_v3 RENAME TO entries;

CREATE INDEX IF NOT EXISTS entries_type_status_idx ON entries(type, status, updated_at);
CREATE INDEX IF NOT EXISTS entries_live_idx ON entries(type, locale, status, published_at);
CREATE INDEX IF NOT EXISTS entries_group_idx ON entries(translation_group);

INSERT OR IGNORE INTO revisions SELECT * FROM _bk_revisions;
INSERT OR IGNORE INTO media_usage SELECT * FROM _bk_media_usage;
INSERT OR IGNORE INTO entry_terms SELECT * FROM _bk_entry_terms;
DROP TABLE _bk_revisions;
DROP TABLE _bk_media_usage;
DROP TABLE _bk_entry_terms;

-- Translated names and archive slugs of categories and tags.
ALTER TABLE terms ADD COLUMN translations TEXT NOT NULL DEFAULT '{}';
