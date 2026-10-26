CREATE TABLE IF NOT EXISTS terms (
	id          INTEGER PRIMARY KEY AUTOINCREMENT,
	taxonomy    TEXT    NOT NULL,                 -- category | tag
	slug        TEXT    NOT NULL,
	name        TEXT    NOT NULL,
	description TEXT    NOT NULL DEFAULT '',
	parent_id   INTEGER REFERENCES terms(id) ON DELETE SET NULL,
	sort        INTEGER NOT NULL DEFAULT 0,
	created_at  INTEGER NOT NULL,
	UNIQUE (taxonomy, slug)
);

CREATE TABLE IF NOT EXISTS entry_terms (
	entry_id INTEGER NOT NULL REFERENCES entries(id) ON DELETE CASCADE,
	term_id  INTEGER NOT NULL REFERENCES terms(id) ON DELETE CASCADE,
	PRIMARY KEY (entry_id, term_id)
);
CREATE INDEX IF NOT EXISTS entry_terms_term_idx ON entry_terms(term_id);

-- The MVP's posts become articles (same table; production had none).
UPDATE entries SET type = 'article' WHERE type = 'post';
