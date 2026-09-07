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

// The only place SQL for entries and revisions lives.
export class EntriesRepo
	db: any
	constructor(@db: any)

	list(type: string, opts: ListOpts = {}): Entry[]
		where := ['type = ?']
		params := [type] as any[]
		if opts.status
			where.push 'status = ?'
			params.push opts.status
		if opts.q
			where.push 'title LIKE ?'
			params.push `%${opts.q}%`
		sql := `SELECT * FROM entries WHERE ${where.join(' AND ')} ORDER BY updated_at DESC LIMIT ? OFFSET ?`
		rows := @db.prepare(sql).all(...params, opts.limit ?? 50, opts.offset ?? 0) as any[]
		rows.map((r) => parse(r) as Entry)

	recent(limit: number): Entry[]
		rows := @db.prepare('SELECT * FROM entries ORDER BY updated_at DESC LIMIT ?').all(limit) as any[]
		rows.map((r) => parse(r) as Entry)

	count(type: string, status?: string): number
		sql := status ? 'SELECT COUNT(*) AS n FROM entries WHERE type = ? AND status = ?' : 'SELECT COUNT(*) AS n FROM entries WHERE type = ?'
		params := status ? [type, status] : [type]
		row := @db.prepare(sql).get(...params) as { n: number }
		row.n

	get(type: string, id: number): Entry | undefined
		parse @db.prepare('SELECT * FROM entries WHERE type = ? AND id = ?').get(type, id)

	bySlug(type: string, slug: string, publishedOnly: boolean): Entry | undefined
		if publishedOnly
			parse @db.prepare('SELECT * FROM entries WHERE type = ? AND slug = ? AND status = ?').get(type, slug, 'published')
		else
			parse @db.prepare('SELECT * FROM entries WHERE type = ? AND slug = ?').get(type, slug)

	slugTaken(type: string, slug: string, excludeId: number | null): boolean
		row := @db.prepare('SELECT id FROM entries WHERE type = ? AND slug = ? AND id != ?').get(type, slug, excludeId ?? -1)
		!!row

	insert(input: { type: string, title: string, slug: string, data: Record<string, any>, authorId: number | null }): number
		now := Date.now()
		result := @db
			.prepare 'INSERT INTO entries (type, slug, status, title, data, author_id, created_at, updated_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?)'
			.run input.type, input.slug, 'draft', input.title, JSON.stringify(input.data), input.authorId, now, now
		Number(result.lastInsertRowid)

	update(id: number, input: { title: string, slug: string, data: Record<string, any> }): void
		@db
			.prepare 'UPDATE entries SET title = ?, slug = ?, data = ?, updated_at = ? WHERE id = ?'
			.run input.title, input.slug, JSON.stringify(input.data), Date.now(), id

	setStatus(id: number, status: string): void
		now := Date.now()
		if status === 'published'
			@db
				.prepare 'UPDATE entries SET status = ?, published_at = COALESCE(published_at, ?), updated_at = ? WHERE id = ?'
				.run status, now, now, id
		else
			@db.prepare('UPDATE entries SET status = ?, updated_at = ? WHERE id = ?').run(status, now, id)

	remove(id: number): void
		@db.prepare('DELETE FROM entries WHERE id = ?').run(id)

	addRevision(entryId: number, data: Record<string, any>, authorId: number | null): void
		@db
			.prepare 'INSERT INTO revisions (entry_id, data, author_id, created_at) VALUES (?, ?, ?, ?)'
			.run entryId, JSON.stringify(data), authorId, Date.now()

	revisions(entryId: number): { id: number, data: Record<string, any>, author_id: number | null, created_at: number }[]
		rows := @db.prepare('SELECT * FROM revisions WHERE entry_id = ? ORDER BY created_at DESC LIMIT 50').all(entryId) as any[]
		rows.map((r) => ({ ...r, data: JSON.parse(r.data || '{}') }))
