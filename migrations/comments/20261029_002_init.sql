-- Comments on entries (pages, articles, any type with comments on).
-- One row per comment; replies point at their parent. Comments belong to
-- one language version of an entry.
CREATE TABLE IF NOT EXISTS comments (
	id           INTEGER PRIMARY KEY AUTOINCREMENT,
	entry_id     INTEGER NOT NULL,
	parent_id    INTEGER,
	depth        INTEGER NOT NULL DEFAULT 0,
	user_id      INTEGER,                         -- signed-in author (member or staff)
	staff        INTEGER NOT NULL DEFAULT 0,      -- written by the team: shown with a badge
	author_name  TEXT    NOT NULL,
	author_email TEXT    NOT NULL DEFAULT '',     -- never shown on the site
	body         TEXT    NOT NULL,
	status       TEXT    NOT NULL DEFAULT 'pending', -- pending | approved | spam | trash
	locale       TEXT    NOT NULL DEFAULT '',
	ip_hash      TEXT    NOT NULL DEFAULT '',
	created_at   INTEGER NOT NULL,
	updated_at   INTEGER NOT NULL,
	approved_at  INTEGER,
	approved_by  INTEGER
);

CREATE INDEX IF NOT EXISTS comments_entry ON comments (entry_id, status, created_at);
CREATE INDEX IF NOT EXISTS comments_status ON comments (status, created_at);
CREATE INDEX IF NOT EXISTS comments_user ON comments (user_id);

-- Editors moderate comments.
UPDATE roles SET capabilities = json_insert(capabilities, '$[#]', 'comments.moderate') WHERE id = 'editor' AND capabilities NOT LIKE '%comments.%';
