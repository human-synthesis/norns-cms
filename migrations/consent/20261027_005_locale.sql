-- Which language's banner the visitor answered (proof of consent).
ALTER TABLE consent_records ADD COLUMN locale TEXT NOT NULL DEFAULT '';
