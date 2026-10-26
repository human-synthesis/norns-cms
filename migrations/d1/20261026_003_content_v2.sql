-- mirrored from migrations/content/20261026_003_v2.sql by tools/d1-sync.js; edit the source, not this copy
-- Visibility (public | members), cached rendered HTML per field, and an index
-- for the public "live" query (published AND published_at <= now).
ALTER TABLE entries ADD COLUMN visibility TEXT NOT NULL DEFAULT 'public';
ALTER TABLE entries ADD COLUMN rendered TEXT NOT NULL DEFAULT '{}';
ALTER TABLE revisions ADD COLUMN title TEXT NOT NULL DEFAULT '';
ALTER TABLE revisions ADD COLUMN slug TEXT NOT NULL DEFAULT '';
CREATE INDEX IF NOT EXISTS entries_live_idx ON entries(type, status, published_at);
