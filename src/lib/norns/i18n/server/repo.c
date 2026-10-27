export class I18nRepo
	db: any
	constructor(@db: any)

	parseLanguage(r: any)
		r ? { ...r, paths: JSON.parse(r.paths || '{}'), enabled: !!r.enabled } : undefined

	parseMarket(r: any)
		return undefined unless r
		{
			id: r.id
			code: r.code
			name: r.name
			mode: r.mode
			prefix: r.prefix
			host: r.host
			languages: JSON.parse(r.languages || '[]')
			defaultLanguage: r.default_language
			prefixDefault: !!r.prefix_default
			countries: JSON.parse(r.countries || '[]')
			contentFallback: !!r.content_fallback
			isPrimary: !!r.is_primary
			enabled: !!r.enabled
			sort: r.sort
		}

	async languages(): Promise<any[]>
		rows := await @db.all 'SELECT * FROM languages ORDER BY sort, code', []
		rows.map((r: any) => @parseLanguage(r))

	async saveLanguage(l: any): Promise<void>
		await @db.run 'INSERT INTO languages (code, name, native, dir, fallback, paths, enabled, sort) VALUES (?, ?, ?, ?, ?, ?, ?, ?) ON CONFLICT(code) DO UPDATE SET name = excluded.name, native = excluded.native, dir = excluded.dir, fallback = excluded.fallback, paths = excluded.paths, enabled = excluded.enabled, sort = excluded.sort', [l.code, l.name, l.native, l.dir, l.fallback, JSON.stringify(l.paths ?? {}), l.enabled ? 1 : 0, l.sort ?? 0]
		return

	async deleteLanguage(code: string): Promise<void>
		await @db.run 'DELETE FROM languages WHERE code = ?', [code]
		await @db.run 'DELETE FROM strings WHERE locale = ?', [code]
		return

	async entriesIn(code: string): Promise<number>
		row := await @db.get 'SELECT COUNT(*) AS n FROM entries WHERE locale = ?', [code]
		Number(row?.n ?? 0)

	async markets(): Promise<any[]>
		rows := await @db.all 'SELECT * FROM localisations ORDER BY is_primary DESC, sort, id', []
		rows.map((r: any) => @parseMarket(r))

	async market(id: number)
		@parseMarket await @db.get('SELECT * FROM localisations WHERE id = ?', [id])

	async saveMarket(id: number | null, m: any): Promise<number>
		values := [m.code, m.name, m.mode, m.prefix, m.host, JSON.stringify(m.languages), m.defaultLanguage, m.prefixDefault ? 1 : 0, JSON.stringify(m.countries ?? []), m.contentFallback ? 1 : 0, m.isPrimary ? 1 : 0, m.enabled ? 1 : 0, m.sort ?? 0]
		if id
			await @db.run 'UPDATE localisations SET code = ?, name = ?, mode = ?, prefix = ?, host = ?, languages = ?, default_language = ?, prefix_default = ?, countries = ?, content_fallback = ?, is_primary = ?, enabled = ?, sort = ? WHERE id = ?', [...values, id]
			return id
		r := await @db.run 'INSERT INTO localisations (code, name, mode, prefix, host, languages, default_language, prefix_default, countries, content_fallback, is_primary, enabled, sort, created_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)', [...values, Date.now()]
		r.lastInsertRowid

	async clearPrimary(exceptId: number): Promise<void>
		await @db.run 'UPDATE localisations SET is_primary = 0 WHERE id != ?', [exceptId]
		return

	async deleteMarket(id: number): Promise<void>
		await @db.run 'DELETE FROM localisations WHERE id = ? AND is_primary = 0', [id]
		return

	async strings(locales: string[]): Promise<any[]>
		return [] unless locales.length
		await @db.all `SELECT locale, key, value FROM strings WHERE locale IN (${locales.map(=> '?').join(',')})`, locales

	async setString(locale: string, key: string, value: string): Promise<void>
		if value === ''
			await @db.run 'DELETE FROM strings WHERE locale = ? AND key = ?', [locale, key]
		else
			await @db.run 'INSERT INTO strings (locale, key, value, updated_at) VALUES (?, ?, ?, ?) ON CONFLICT(locale, key) DO UPDATE SET value = excluded.value, updated_at = excluded.updated_at', [locale, key, value, Date.now()]
		return
