export type Media =
	id: number
	key: string
	filename: string
	mime: string
	size: number
	width: number | null
	height: number | null
	alt: string
	caption: string
	author_id: number | null
	created_at: number

export class MediaRepo
	db: any
	constructor(@db: any)

	async insert(m: Omit<Media, 'id' | 'created_at'>): Promise<number>
		r := await @db.run 'INSERT INTO media (key, filename, mime, size, width, height, alt, caption, author_id, created_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)', [m.key, m.filename, m.mime, m.size, m.width, m.height, m.alt, m.caption, m.author_id, Date.now()]
		r.lastInsertRowid

	async get(id: number): Promise<Media | undefined>
		await @db.get 'SELECT * FROM media WHERE id = ?', [id]

	async byKey(key: string): Promise<Media | undefined>
		await @db.get 'SELECT * FROM media WHERE key = ?', [key]

	async byIds(ids: number[]): Promise<Media[]>
		return [] unless ids.length
		await @db.all `SELECT * FROM media WHERE id IN (${ids.map(=> '?').join(',')})`, ids

	async list(q: { q?: string, kind?: string, limit?: number, offset?: number } = {}): Promise<any[]>
		where := [] as string[]
		params := [] as any[]
		if q.q
			where.push '(filename LIKE ? OR alt LIKE ? OR caption LIKE ?)'
			params.push `%${q.q}%`, `%${q.q}%`, `%${q.q}%`
		if q.kind === 'image'
			where.push "mime LIKE 'image/%'"
		else if q.kind === 'document'
			where.push "mime NOT LIKE 'image/%'"
		sql := where.length ? `WHERE ${where.join(' AND ')}` : ''
		await @db.all `SELECT m.*, (SELECT COUNT(*) FROM media_usage u WHERE u.media_id = m.id) AS usage FROM media m ${sql} ORDER BY m.id DESC LIMIT ? OFFSET ?`, [...params, q.limit ?? 60, q.offset ?? 0]

	async count(): Promise<number>
		row := await @db.get 'SELECT COUNT(*) AS n FROM media', []
		Number(row?.n ?? 0)

	async update(id: number, fields: { alt?: string, caption?: string, filename?: string }): Promise<void>
		await @db.run 'UPDATE media SET alt = COALESCE(?, alt), caption = COALESCE(?, caption), filename = COALESCE(?, filename) WHERE id = ?', [fields.alt ?? null, fields.caption ?? null, fields.filename ?? null, id]
		return

	async remove(id: number): Promise<void>
		await @db.run 'DELETE FROM media WHERE id = ?', [id]
		return

	async setUsage(entryId: number, mediaIds: number[]): Promise<void>
		await @db.run 'DELETE FROM media_usage WHERE entry_id = ?', [entryId]
		for id of mediaIds
			await @db.run 'INSERT OR IGNORE INTO media_usage (media_id, entry_id) SELECT ?, ? WHERE EXISTS (SELECT 1 FROM media WHERE id = ?)', [id, entryId, id]
		return

	async usage(mediaId: number): Promise<any[]>
		await @db.all 'SELECT e.id, e.type, e.title, e.slug FROM media_usage u JOIN entries e ON e.id = u.entry_id WHERE u.media_id = ?', [mediaId]
