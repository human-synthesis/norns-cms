-- Country filter (protection/server/countries.c): settings as JSON in
-- protection_state, and how many requests each country had refused.
CREATE TABLE IF NOT EXISTS protection_state (
	key        TEXT    PRIMARY KEY,
	value      TEXT    NOT NULL,
	updated_at INTEGER NOT NULL,
	updated_by INTEGER
);

CREATE TABLE IF NOT EXISTS country_blocks (
	country     TEXT    PRIMARY KEY,
	hits        INTEGER NOT NULL DEFAULT 0,
	last_hit_at INTEGER
);
