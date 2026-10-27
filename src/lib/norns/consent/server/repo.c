export class ConsentRepo
	db: any
	constructor(@db: any)

	async scripts(enabledOnly = false): Promise<any[]>
		sql := enabledOnly ? 'SELECT * FROM consent_scripts WHERE enabled = 1 ORDER BY sort, id' : 'SELECT * FROM consent_scripts ORDER BY sort, id'
		rows := await @db.all sql, []
		rows.map((r: any) => ({ ...r, config: JSON.parse(r.config || '{}'), enabled: !!r.enabled }))

	async script(id: number): Promise<any>
		r := await @db.get 'SELECT * FROM consent_scripts WHERE id = ?', [id]
		r ? { ...r, config: JSON.parse(r.config || '{}'), enabled: !!r.enabled } : undefined

	async saveScript(id: number | null, s: Record<string, any>, userId: number | null): Promise<number>
		now := Date.now()
		if id
			await @db.run 'UPDATE consent_scripts SET name = ?, provider = ?, category = ?, placement = ?, config = ?, code = ?, cookies = ?, enabled = ?, sort = ?, updated_at = ?, updated_by = ? WHERE id = ?', [s.name, s.provider, s.category, s.placement, JSON.stringify(s.config), s.code, s.cookies, s.enabled ? 1 : 0, s.sort, now, userId, id]
			return id
		r := await @db.run 'INSERT INTO consent_scripts (name, provider, category, placement, config, code, cookies, enabled, sort, updated_at, updated_by) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)', [s.name, s.provider, s.category, s.placement, JSON.stringify(s.config), s.code, s.cookies, s.enabled ? 1 : 0, s.sort, now, userId]
		r.lastInsertRowid

	async removeScript(id: number): Promise<void>
		await @db.run 'DELETE FROM consent_scripts WHERE id = ?', [id]
		return

	async record(r: Record<string, any>): Promise<void>
		await @db.run 'INSERT INTO consent_records (consent_id, user_id, policy_version, action, categories, country, region_mode, gpc, banner_hash, locale, created_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)', [r.consentId, r.userId ?? null, r.version, r.action, JSON.stringify(r.choice), r.country ?? '', r.mode ?? '', r.gpc ? 1 : 0, r.bannerHash ?? '', r.locale ?? '', Date.now()]
		return

	async records(q: { consentId?: string, userId?: number, action?: string, limit?: number, offset?: number } = {}): Promise<any[]>
		where := [] as string[]
		params := [] as any[]
		if q.consentId
			where.push 'consent_id = ?'
			params.push q.consentId
		if q.userId
			where.push 'user_id = ?'
			params.push q.userId
		if q.action
			where.push 'action = ?'
			params.push q.action
		sql := where.length ? `WHERE ${where.join(' AND ')}` : ''
		rows := await @db.all `SELECT * FROM consent_records ${sql} ORDER BY id DESC LIMIT ? OFFSET ?`, [...params, q.limit ?? 100, q.offset ?? 0]
		rows.map((r: any) => ({ ...r, categories: JSON.parse(r.categories || '{}'), gpc: !!r.gpc }))

	async stats(since: number): Promise<any>
		byAction := await @db.all 'SELECT action, COUNT(*) AS n FROM consent_records WHERE created_at >= ? GROUP BY action', [since]
		cats := await @db.get "SELECT COUNT(*) AS total, SUM(CASE WHEN json_extract(categories, '$.analytics') THEN 1 ELSE 0 END) AS analytics, SUM(CASE WHEN json_extract(categories, '$.marketing') THEN 1 ELSE 0 END) AS marketing, SUM(CASE WHEN json_extract(categories, '$.preferences') THEN 1 ELSE 0 END) AS preferences FROM consent_records WHERE created_at >= ? AND action != 'withdraw'", [since]
		byCountry := await @db.all "SELECT country, COUNT(*) AS n FROM consent_records WHERE created_at >= ? AND country != '' GROUP BY country ORDER BY n DESC LIMIT 10", [since]
		{ byAction, cats, byCountry }

	async purge(before: number): Promise<number>
		r := await @db.run 'DELETE FROM consent_records WHERE created_at < ?', [before]
		r.changes

	async detachUser(userId: number): Promise<void>
		await @db.run 'UPDATE consent_records SET user_id = NULL WHERE user_id = ?', [userId]
		return
