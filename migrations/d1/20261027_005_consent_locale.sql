-- mirrored from migrations/consent/20261027_005_locale.sql by tools/d1-sync.js; edit the source, not this copy
-- Which language's banner the visitor answered (proof of consent).
ALTER TABLE consent_records ADD COLUMN locale TEXT NOT NULL DEFAULT '';
