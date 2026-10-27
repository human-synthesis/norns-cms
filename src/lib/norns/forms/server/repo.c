parseForm := (r: any) =>
	return undefined unless r
	{ ...r, fields: JSON.parse(r.fields || '[]'), settings: JSON.parse(r.settings || '{}'), translations: JSON.parse(r.translations || '{}'), enabled: !!r.enabled }

parseSubmission := (r: any) => r ? { ...r, data: JSON.parse(r.data || '{}') } : undefined

export class FormsRepo
	db: any
	constructor(@db: any)

	async list(): Promise<any[]>
		rows := await @db.all "SELECT f.*, (SELECT COUNT(*) FROM form_submissions s WHERE s.form_id = f.id AND s.status = 'new') AS unread, (SELECT COUNT(*) FROM form_submissions s WHERE s.form_id = f.id AND s.status != 'spam') AS total FROM forms f ORDER BY CASE f.kind WHEN 'contact' THEN 0 WHEN 'newsletter' THEN 1 ELSE 2 END, f.name COLLATE NOCASE", []
		rows.map((r: any) => ({ ...parseForm(r), unread: Number(r.unread), total: Number(r.total) }))

	async get(id: number)
		parseForm await @db.get('SELECT * FROM forms WHERE id = ?', [id])

	async bySlug(slug: string)
		parseForm await @db.get('SELECT * FROM forms WHERE slug = ?', [slug])

	async slugTaken(slug: string, excludeId: number): Promise<boolean>
		!!(await @db.get 'SELECT id FROM forms WHERE slug = ? AND id != ?', [slug, excludeId])

	async save(id: number | null, f: any): Promise<number>
		now := Date.now()
		values := [f.slug, f.name, f.kind, JSON.stringify(f.fields), JSON.stringify(f.settings), JSON.stringify(f.translations), f.enabled ? 1 : 0]
		if id
			await @db.run 'UPDATE forms SET slug = ?, name = ?, kind = ?, fields = ?, settings = ?, translations = ?, enabled = ?, updated_at = ? WHERE id = ?', [...values, now, id]
			return id
		r := await @db.run 'INSERT INTO forms (slug, name, kind, fields, settings, translations, enabled, created_at, updated_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)', [...values, now, now]
		r.lastInsertRowid

	async remove(id: number): Promise<void>
		await @db.run 'DELETE FROM forms WHERE id = ?', [id]
		return

	async insertSubmission(s: any): Promise<number>
		r := await @db.run 'INSERT INTO form_submissions (form_id, data, email, status, locale, page, user_id, created_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?)', [s.formId, JSON.stringify(s.data), s.email, s.status, s.locale, s.page, s.userId ?? null, Date.now()]
		r.lastInsertRowid

	async setNotified(id: number, state: number, error: string): Promise<void>
		await @db.run 'UPDATE form_submissions SET notified = ?, notify_error = ? WHERE id = ?', [state, error, id]
		return

	async hasEmail(formId: number, email: string): Promise<boolean>
		!!(await @db.get "SELECT id FROM form_submissions WHERE form_id = ? AND email = ? AND status != 'spam'", [formId, email])

	where(q: any)
		where := [] as string[]
		params := [] as any[]
		if q.formId
			where.push 's.form_id = ?'
			params.push q.formId
		if q.status
			where.push 's.status = ?'
			params.push q.status
		else
			where.push "s.status != 'spam'"
		if q.q
			where.push '(s.email LIKE ? OR s.data LIKE ?)'
			params.push `%${q.q}%`, `%${q.q}%`
		{ sql: where.join(' AND '), params }

	async submissions(q: any = {}): Promise<any[]>
		{ sql, params } := @where q
		rows := await @db.all `SELECT s.*, f.name AS form_name, f.slug AS form_slug FROM form_submissions s JOIN forms f ON f.id = s.form_id WHERE ${sql} ORDER BY s.id DESC LIMIT ? OFFSET ?`, [...params, q.limit ?? 50, q.offset ?? 0]
		rows.map(parseSubmission)

	async count(q: any = {}): Promise<number>
		{ sql, params } := @where q
		row := await @db.get `SELECT COUNT(*) AS n FROM form_submissions s WHERE ${sql}`, params
		Number(row?.n ?? 0)

	async submission(id: number)
		parseSubmission await @db.get('SELECT s.*, f.name AS form_name, f.slug AS form_slug FROM form_submissions s JOIN forms f ON f.id = s.form_id WHERE s.id = ?', [id])

	async setStatus(ids: number[], status: string): Promise<void>
		return unless ids.length
		await @db.run `UPDATE form_submissions SET status = ? WHERE id IN (${ids.map(=> '?').join(',')})`, [status, ...ids]
		return

	async removeSubmissions(ids: number[]): Promise<void>
		return unless ids.length
		await @db.run `DELETE FROM form_submissions WHERE id IN (${ids.map(=> '?').join(',')})`, ids
		return

	async purge(before: number): Promise<number>
		r := await @db.run 'DELETE FROM form_submissions WHERE created_at < ?', [before]
		r.changes
