// protection_config (one row), protection_forms (per-form switches) and
// protection_stats (daily outcome counts); see migrations/protection.
export class ProtectionRepo
	db: any
	constructor(@db: any)

	config()
		@db.get 'SELECT * FROM protection_config WHERE id = 1', []

	async saveConfig(values: Record<string, any>, userId: number | null)
		await @db.run 'INSERT INTO protection_config (id, enabled, site_key, secret, appearance, verified_at, updated_at, updated_by) VALUES (1, ?, ?, ?, ?, ?, ?, ?) ON CONFLICT(id) DO UPDATE SET enabled = excluded.enabled, site_key = excluded.site_key, secret = excluded.secret, appearance = excluded.appearance, verified_at = excluded.verified_at, updated_at = excluded.updated_at, updated_by = excluded.updated_by', [values.enabled ? 1 : 0, values.site_key ?? '', values.secret ?? null, values.appearance ?? 'always', values.verified_at ?? null, Date.now(), userId]
		return

	overrides()
		@db.all 'SELECT * FROM protection_forms', []

	async saveForm(formId: string, turnstile: string, honeypot: boolean)
		await @db.run 'INSERT INTO protection_forms (form_id, turnstile, honeypot, updated_at) VALUES (?, ?, ?, ?) ON CONFLICT(form_id) DO UPDATE SET turnstile = excluded.turnstile, honeypot = excluded.honeypot, updated_at = excluded.updated_at', [formId, turnstile, honeypot ? 1 : 0, Date.now()]
		return

	async count(formId: string, outcome: string)
		day := new Date().toISOString().slice(0, 10)
		await @db.run 'INSERT INTO protection_stats (form_id, day, outcome, count) VALUES (?, ?, ?, 1) ON CONFLICT(form_id, day, outcome) DO UPDATE SET count = count + 1', [formId, day, outcome]
		return

	stats(sinceDay: string)
		@db.all 'SELECT form_id, outcome, SUM(count) AS n FROM protection_stats WHERE day >= ? GROUP BY form_id, outcome', [sinceDay]

	async purge(beforeDay: string)
		r := await @db.run 'DELETE FROM protection_stats WHERE day < ?', [beforeDay]
		r.changes

	// --- blocked paths -------------------------------------------------------

	async blockedPaths(): Promise<any[]>
		await @db.all 'SELECT * FROM blocked_paths ORDER BY hits DESC, pattern', []

	async blockedPath(id: number): Promise<any>
		await @db.get 'SELECT * FROM blocked_paths WHERE id = ?', [id]

	async blockedByPattern(pattern: string): Promise<any>
		await @db.get 'SELECT * FROM blocked_paths WHERE pattern = ?', [pattern]

	async addBlockedPath(pattern: string, note: string, source: string, userId: number | null): Promise<void>
		await @db.run 'INSERT OR IGNORE INTO blocked_paths (pattern, note, source, created_by, created_at) VALUES (?, ?, ?, ?, ?)', [pattern, note, source, userId, Date.now()]
		return

	async updateBlockedPath(id: number, fields: { enabled?: boolean, note?: string }): Promise<void>
		await @db.run 'UPDATE blocked_paths SET enabled = ? WHERE id = ?', [fields.enabled ? 1 : 0, id] if fields.enabled !== undefined
		await @db.run 'UPDATE blocked_paths SET note = ? WHERE id = ?', [fields.note, id] if fields.note !== undefined
		return

	async removeBlockedPath(id: number): Promise<void>
		await @db.run 'DELETE FROM blocked_paths WHERE id = ?', [id]
		return

	async countBlockedHits(id: number, n: number, at: number): Promise<void>
		await @db.run 'UPDATE blocked_paths SET hits = hits + ?, last_hit_at = MAX(COALESCE(last_hit_at, 0), ?) WHERE id = ?', [n, at, id]
		return

	// --- country filter ------------------------------------------------------

	async state(key: string): Promise<string | null>
		row := await @db.get 'SELECT value FROM protection_state WHERE key = ?', [key]
		row?.value ?? null

	async setState(key: string, value: string, userId: number | null): Promise<void>
		await @db.run 'INSERT INTO protection_state (key, value, updated_at, updated_by) VALUES (?, ?, ?, ?) ON CONFLICT(key) DO UPDATE SET value = excluded.value, updated_at = excluded.updated_at, updated_by = excluded.updated_by', [key, value, Date.now(), userId]
		return

	async countCountryBlocks(country: string, n: number, at: number): Promise<void>
		await @db.run 'INSERT INTO country_blocks (country, hits, last_hit_at) VALUES (?, ?, ?) ON CONFLICT(country) DO UPDATE SET hits = hits + excluded.hits, last_hit_at = MAX(COALESCE(last_hit_at, 0), excluded.last_hit_at)', [country, n, at]
		return

	async countryBlocks(): Promise<any[]>
		await @db.all 'SELECT * FROM country_blocks ORDER BY hits DESC', []

	async clearCountryBlocks(): Promise<void>
		await @db.run 'DELETE FROM country_blocks', []
		return
