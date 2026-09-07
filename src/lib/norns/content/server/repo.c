export type EntryRow =
	id: number
	type: string
	slug: string
	status: string
	title: string
	data: string
	author_id: number | null
	published_at: number | null
	created_at: number
	updated_at: number

export type Entry = Omit<EntryRow, 'data'> & { data: Record<string, any> }

export type ListOpts =
	status?: string
	q?: string
	limit?: number
	offset?: number

parse := (row: any): Entry | undefined =>
	return undefined unless row
	{ ...row, data: JSON.parse(row.data || '{}') }

// The only place SQL for entries and revisions lives. `db` is the async
// surface from auth/server/db.js (SQLite locally, D1 on Workers).
export class EntriesRepo
	db: any
	constructor(@db: any)

	async list(type: string, opts: ListOpts = {}): Promise<Entry[]>
		where := ['type = ?']
		params := [type] as any[]
		if opts.status
			where.push 'status = ?'
			params.push opts.status
		if opts.q
			where.push 'title LIKE ?'
			params.push `%${opts.q}%`
		sql := `SELECT * FROM entries WHERE ${where.join(' AND ')} ORDER BY updated_at DESC LIMIT ? OFFSET ?`
		rows := await @db.all sql, [...params, opts.limit ?? 50, opts.offset ?? 0]
		rows.map((r: any) => parse(r) as Entry)

	async recent(limit: number): Promise<Entry[]>
		rows := await @db.all 'SELECT * FROM entries ORDER BY updated_at DESC LIMIT ?', [limit]
		rows.map((r: any) => parse(r) as Entry)

	async count(type: string, status?: string): Promise<number>
		sql := status ? 'SELECT COUNT(*) AS n FROM entries WHERE type = ? AND status = ?' : 'SELECT COUNT(*) AS n FROM entries WHERE type = ?'
		params := status ? [type, status] : [type]
		row := await @db.get sql, params
		Number(row?.n ?? 0)

	async get(type: string, id: number): Promise<Entry | undefined>
		parse await @db.get('SELECT * FROM entries WHERE type = ? AND id = ?', [type, id])

	async bySlug(type: string, slug: string, publishedOnly: boolean): Promise<Entry | undefined>
		if publishedOnly
			parse await @db.get('SELECT * FROM entries WHERE type = ? AND slug = ? AND status = ?', [type, slug, 'published'])
		else
			parse await @db.get('SELECT * FROM entries WHERE type = ? AND slug = ?', [type, slug])

	async slugTaken(type: string, slug: string, excludeId: number | null): Promise<boolean>
		row := await @db.get 'SELECT id FROM entries WHERE type = ? AND slug = ? AND id != ?', [type, slug, excludeId ?? -1]
		!!row

	async insert(input: { type: string, title: string, slug: string, data: Record<string, any>, authorId: number | null }): Promise<number>
		now := Date.now()
		result := await @db.run 'INSERT INTO entries (type, slug, status, title, data, author_id, created_at, updated_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?)', [input.type, input.slug, 'draft', input.title, JSON.stringify(input.data), input.authorId, now, now]
		result.lastInsertRowid

	async update(id: number, input: { title: string, slug: string, data: Record<string, any> }): Promise<void>
		await @db.run 'UPDATE entries SET title = ?, slug = ?, data = ?, updated_at = ? WHERE id = ?', [input.title, input.slug, JSON.stringify(input.data), Date.now(), id]
		return

	async setStatus(id: number, status: string): Promise<void>
		now := Date.now()
		if status === 'published'
			await @db.run 'UPDATE entries SET status = ?, published_at = COALESCE(published_at, ?), updated_at = ? WHERE id = ?', [status, now, now, id]
		else
			await @db.run 'UPDATE entries SET status = ?, updated_at = ? WHERE id = ?', [status, now, id]
		return

	async remove(id: number): Promise<void>
		await @db.run 'DELETE FROM entries WHERE id = ?', [id]
		return

	async addRevision(entryId: number, data: Record<string, any>, authorId: number | null): Promise<void>
		await @db.run 'INSERT INTO revisions (entry_id, data, author_id, created_at) VALUES (?, ?, ?, ?)', [entryId, JSON.stringify(data), authorId, Date.now()]
		return

	async revisions(entryId: number): Promise<{ id: number, data: Record<string, any>, author_id: number | null, created_at: number }[]>
		rows := await @db.all 'SELECT * FROM revisions WHERE entry_id = ? ORDER BY created_at DESC LIMIT 50', [entryId]
		rows.map((r: any) => ({ ...r, data: JSON.parse(r.data || '{}') }))
