-- The language a member uses the site in (their emails follow it).
ALTER TABLE users ADD COLUMN locale TEXT NOT NULL DEFAULT 'en';
