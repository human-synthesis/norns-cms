-- mirrored from migrations/auth/20261027_004_locale.sql by tools/d1-sync.js; edit the source, not this copy
-- The language a member uses the site in (their emails follow it).
ALTER TABLE users ADD COLUMN locale TEXT NOT NULL DEFAULT 'en';
