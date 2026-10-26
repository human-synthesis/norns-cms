-- Users v2: staff and client users, roles with capabilities, tokens (invite,
-- verify, reset, 2FA login), audit log and login rate limiting.
ALTER TABLE users ADD COLUMN kind TEXT NOT NULL DEFAULT 'staff';          -- staff | client
ALTER TABLE users ADD COLUMN status TEXT NOT NULL DEFAULT 'active';       -- invited | active | suspended | deleted
ALTER TABLE users ADD COLUMN slug TEXT;                                   -- author pages (staff)
ALTER TABLE users ADD COLUMN bio TEXT NOT NULL DEFAULT '';
ALTER TABLE users ADD COLUMN avatar_media_id INTEGER;
ALTER TABLE users ADD COLUMN email_verified_at INTEGER;
ALTER TABLE users ADD COLUMN last_login_at INTEGER;
ALTER TABLE users ADD COLUMN totp_secret TEXT;
ALTER TABLE users ADD COLUMN totp_enabled INTEGER NOT NULL DEFAULT 0;
ALTER TABLE users ADD COLUMN recovery_codes TEXT NOT NULL DEFAULT '[]';
ALTER TABLE users ADD COLUMN password_changed_at INTEGER;
ALTER TABLE users ADD COLUMN deleted_at INTEGER;

CREATE UNIQUE INDEX IF NOT EXISTS users_slug_idx ON users(slug) WHERE slug IS NOT NULL;
CREATE INDEX IF NOT EXISTS users_kind_idx ON users(kind, status);

-- The MVP had admin / editor. The seeded admin becomes the site owner.
UPDATE users SET role = 'owner' WHERE role = 'admin';
UPDATE users SET email_verified_at = created_at WHERE email_verified_at IS NULL;

ALTER TABLE sessions ADD COLUMN user_agent TEXT NOT NULL DEFAULT '';
ALTER TABLE sessions ADD COLUMN last_seen_at INTEGER;

CREATE TABLE IF NOT EXISTS roles (
	id           TEXT    PRIMARY KEY,
	kind         TEXT    NOT NULL,                 -- staff | client
	label        TEXT    NOT NULL,
	description  TEXT    NOT NULL DEFAULT '',
	capabilities TEXT    NOT NULL DEFAULT '[]',   -- JSON array; '*' and 'prefix.*' wildcards
	builtin      INTEGER NOT NULL DEFAULT 0,
	created_at   INTEGER NOT NULL
);

INSERT OR IGNORE INTO roles (id, kind, label, description, capabilities, builtin, created_at) VALUES
	('owner',  'staff',  'Owner',  'Full access, including other owners', '["*"]', 1, 0),
	('admin',  'staff',  'Administrator', 'Full access except managing owners', '["*"]', 1, 0),
	('editor', 'staff',  'Editor', 'All content, media, taxonomies and SEO; publishes', '["admin.access","content.*","media.*","taxonomy.manage","seo.manage","consent.view","users.view"]', 1, 0),
	('author', 'staff',  'Author', 'Writes own articles and submits them for review', '["admin.access","content.view","content.create","content.edit_own","media.view","media.upload"]', 1, 0),
	('viewer', 'staff',  'Viewer', 'Read-only access to the admin', '["admin.access","content.view","media.view","consent.view","users.view","audit.view"]', 1, 0),
	('member', 'client', 'Member', 'Site member: own account and members-only content', '["account.access","content.members"]', 1, 0);

CREATE TABLE IF NOT EXISTS tokens (
	id         TEXT    PRIMARY KEY,               -- sha256 of the raw token; the raw value only travels in the link
	user_id    INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
	purpose    TEXT    NOT NULL,                  -- invite | verify | reset | mfa
	expires_at INTEGER NOT NULL,
	used_at    INTEGER,
	created_at INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS tokens_user_idx ON tokens(user_id, purpose);

CREATE TABLE IF NOT EXISTS audit_log (
	id         INTEGER PRIMARY KEY AUTOINCREMENT,
	actor_id   INTEGER,
	actor      TEXT    NOT NULL DEFAULT '',       -- email at the time, survives user deletion
	action     TEXT    NOT NULL,
	target     TEXT    NOT NULL DEFAULT '',
	meta       TEXT    NOT NULL DEFAULT '{}',
	created_at INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS audit_created_idx ON audit_log(created_at);
CREATE INDEX IF NOT EXISTS audit_actor_idx ON audit_log(actor_id, created_at);

CREATE TABLE IF NOT EXISTS rate_limits (
	key          TEXT    PRIMARY KEY,             -- sha256(scope:value); no raw IPs or emails
	count        INTEGER NOT NULL,
	window_start INTEGER NOT NULL
);
