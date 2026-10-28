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
