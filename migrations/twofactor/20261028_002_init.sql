-- Two-factor sign-in moves out of the users table into its own feature.
-- One row per user who started or finished setup; enabled_at is null while
-- the setup waits for its first code. recovery_codes holds SHA-256 hashes.
CREATE TABLE IF NOT EXISTS two_factor (
	user_id        INTEGER PRIMARY KEY REFERENCES users(id) ON DELETE CASCADE,
	secret         TEXT    NOT NULL,
	enabled_at     INTEGER,
	recovery_codes TEXT    NOT NULL DEFAULT '[]',
	updated_at     INTEGER NOT NULL
);

INSERT OR IGNORE INTO two_factor (user_id, secret, enabled_at, recovery_codes, updated_at)
	SELECT id, totp_secret, CASE WHEN totp_enabled = 1 THEN CAST(strftime('%s', 'now') AS INTEGER) * 1000 END, COALESCE(recovery_codes, '[]'), CAST(strftime('%s', 'now') AS INTEGER) * 1000
	FROM users WHERE totp_secret IS NOT NULL;

-- The old "Security → Two-factor sign-in" switch becomes the staff policy.
INSERT OR IGNORE INTO settings ("group", value, updated_at)
	SELECT 'twofactor', json_object('staff', CASE WHEN json_extract(value, '$.require2fa') THEN 'admins' ELSE 'off' END, 'members', 'off'), updated_at
	FROM settings WHERE "group" = 'security';

-- users.totp_secret / totp_enabled / recovery_codes are no longer read or
-- written; they stay one release so a code rollback finds its data.
