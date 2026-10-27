export type EntryRow =
	id: number
	type: string
	locale: string
	slug: string
	status: string
	visibility: string
	title: string
	data: string
	rendered: string
	markets: string
	translation_group: number | null
	translation_of: number | null
	source_updated_at: number | null
	author_id: number | null
	published_at: number | null
	created_at: number
	updated_at: number

export type Entry = Omit<EntryRow, 'data' | 'rendered' | 'markets'> & { data: Record<string, any>, rendered: Record<string, any>, markets: string[] }

export type ListOpts =
	status?: string            // draft | in_review | published | scheduled | live
	q?: string
	limit?: number
	offset?: number
	authorId?: number
	visibility?: string
	termId?: number
	featured?: boolean
	excludeIds?: number[]
	ids?: number[]
	locale?: string            // one language
	market?: string            // only entries available in this localisation
	order?: string             // updated | published | title

parse := (row: any): Entry | undefined =>
	return undefined unless row
	{ ...row, data: JSON.parse(row.data || '{}'), rendered: JSON.parse(row.rendered || '{}'), markets: JSON.parse(row.markets || '[]') }

// "live" = what the public may see: published and not scheduled for later.
LIVE := "status = 'published' AND (published_at IS NULL OR published_at <= ?)"

// The only place SQL for entries and revisions lives. `db` is the async
// surface from auth/server/db.js (SQLite locally, D1 on Workers).
export class EntriesRepo
	db: any
	constructor(@db: any)

	where(type: string | null, opts: ListOpts)
		where := [] as string[]
		params := [] as any[]
		if type
			where.push 'type = ?'
			params.push type
		if opts.status === 'live'
			where.push LIVE
			params.push Date.now()
		else if opts.status === 'scheduled'
			where.push "status = 'published' AND published_at > ?"
			params.push Date.now()
		else if opts.status === 'published'
			where.push LIVE
			params.push Date.now()
		else if opts.status
			where.push 'status = ?'
			params.push opts.status
		if opts.q
			where.push '(title LIKE ? OR slug LIKE ?)'
			params.push `%${opts.q}%`, `%${opts.q}%`
		if opts.authorId
			where.push 'author_id = ?'
			params.push opts.authorId
		if opts.visibility
			where.push 'visibility = ?'
			params.push opts.visibility
		if opts.termId
			where.push 'id IN (SELECT entry_id FROM entry_terms WHERE term_id = ?)'
			params.push opts.termId
		if opts.featured
			where.push "json_extract(data, '$.featured') = 1"
		if opts.excludeIds?.length
			where.push `id NOT IN (${opts.excludeIds.map(=> '?').join(',')})`
			params.push ...opts.excludeIds
		if opts.locale
			where.push 'locale = ?'
			params.push opts.locale
		if opts.market
			where.push "(markets = '[]' OR EXISTS (SELECT 1 FROM json_each(entries.markets) WHERE value = ?))"
			params.push opts.market
		if opts.ids
			return { sql: '1 = 0', params: [] } unless opts.ids.length
			where.push `id IN (${opts.ids.map(=> '?').join(',')})`
			params.push ...opts.ids
		{ sql: where.length ? where.join(' AND ') : '1 = 1', params }

	orderBy(opts: ListOpts)
		return 'COALESCE(published_at, updated_at) DESC, id DESC' if opts.order === 'published'
		return 'title COLLATE NOCASE ASC' if opts.order === 'title'
		'updated_at DESC, id DESC'

	async list(type: string | null, opts: ListOpts = {}): Promise<Entry[]>
		{ sql, params } := @where type, opts
		rows := await @db.all `SELECT * FROM entries WHERE ${sql} ORDER BY ${@orderBy opts} LIMIT ? OFFSET ?`, [...params, opts.limit ?? 50, opts.offset ?? 0]
		rows.map((r: any) => parse(r) as Entry)

	async count(type: string | null, opts: ListOpts = {}): Promise<number>
		{ sql, params } := @where type, opts
		row := await @db.get `SELECT COUNT(*) AS n FROM entries WHERE ${sql}`, params
		Number(row?.n ?? 0)

	async recent(limit: number): Promise<Entry[]>
		rows := await @db.all 'SELECT * FROM entries ORDER BY updated_at DESC LIMIT ?', [limit]
		rows.map((r: any) => parse(r) as Entry)

	async get(type: string, id: number): Promise<Entry | undefined>
		parse await @db.get('SELECT * FROM entries WHERE type = ? AND id = ?', [type, id])

	async byId(id: number): Promise<Entry | undefined>
		parse await @db.get('SELECT * FROM entries WHERE id = ?', [id])

	async bySlug(type: string, slug: string, liveOnly: boolean, locale: string): Promise<Entry | undefined>
		if liveOnly
			parse await @db.get(`SELECT * FROM entries WHERE type = ? AND locale = ? AND slug = ? AND ${LIVE}`, [type, locale, slug, Date.now()])
		else
			parse await @db.get('SELECT * FROM entries WHERE type = ? AND locale = ? AND slug = ?', [type, locale, slug])

	async bySlugs(type: string, slugs: string[], locale: string): Promise<Entry[]>
		return [] unless slugs.length
		rows := await @db.all `SELECT * FROM entries WHERE type = ? AND locale = ? AND slug IN (${slugs.map(=> '?').join(',')}) AND ${LIVE}`, [type, locale, ...slugs, Date.now()]
		rows.map((r: any) => parse(r) as Entry)

	async slugTaken(type: string, locale: string, slug: string, excludeId: number | null): Promise<boolean>
		row := await @db.get 'SELECT id FROM entries WHERE type = ? AND locale = ? AND slug = ? AND id != ?', [type, locale, slug, excludeId ?? -1]
		!!row

	// Every entry of a translation group (any status), oldest first.
	async group(groupId: number): Promise<Entry[]>
		rows := await @db.all 'SELECT * FROM entries WHERE translation_group = ? ORDER BY id', [groupId]
		rows.map((r: any) => parse(r) as Entry)

	async groups(groupIds: number[]): Promise<{ translation_group: number, locale: string, status: string, published_at: number | null, id: number }[]>
		return [] unless groupIds.length
		await @db.all `SELECT id, translation_group, locale, status, published_at FROM entries WHERE translation_group IN (${groupIds.map(=> '?').join(',')})`, groupIds

	async stamp(id: number, sourceUpdatedAt: number | null): Promise<void>
		await @db.run 'UPDATE entries SET source_updated_at = ? WHERE id = ?', [sourceUpdatedAt, id]
		return

	async insert(input: { type: string, locale: string, title: string, slug: string, data: Record<string, any>, rendered: Record<string, any>, visibility: string, markets: string[], authorId: number | null, group?: number | null, translationOf?: number | null, sourceUpdatedAt?: number | null }): Promise<number>
		now := Date.now()
		result := await @db.run 'INSERT INTO entries (type, locale, slug, status, visibility, title, data, rendered, markets, translation_group, translation_of, source_updated_at, author_id, created_at, updated_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)', [input.type, input.locale, input.slug, 'draft', input.visibility, input.title, JSON.stringify(input.data), JSON.stringify(input.rendered), JSON.stringify(input.markets ?? []), input.group ?? null, input.translationOf ?? null, input.sourceUpdatedAt ?? null, input.authorId, now, now]
		id := result.lastInsertRowid
		unless input.group
			await @db.run 'UPDATE entries SET translation_group = ? WHERE id = ?', [id, id]
		id

	async update(id: number, input: { title: string, slug: string, data: Record<string, any>, rendered: Record<string, any>, visibility: string, markets: string[] }): Promise<void>
		await @db.run 'UPDATE entries SET title = ?, slug = ?, data = ?, rendered = ?, visibility = ?, markets = ?, updated_at = ? WHERE id = ?', [input.title, input.slug, JSON.stringify(input.data), JSON.stringify(input.rendered), input.visibility, JSON.stringify(input.markets ?? []), Date.now(), id]
		return

	async setRendered(id: number, rendered: Record<string, any>): Promise<void>
		await @db.run 'UPDATE entries SET rendered = ? WHERE id = ?', [JSON.stringify(rendered), id]
		return

	async setAuthor(id: number, authorId: number | null): Promise<void>
		await @db.run 'UPDATE entries SET author_id = ? WHERE id = ?', [authorId, id]
		return

	async reassignAuthor(fromId: number, toId: number | null): Promise<void>
		await @db.run 'UPDATE entries SET author_id = ? WHERE author_id = ?', [toId, fromId]
		return

	async setStatus(id: number, status: string, publishAt: number | null): Promise<void>
		now := Date.now()
		if status === 'published'
			await @db.run 'UPDATE entries SET status = ?, published_at = ?, updated_at = ? WHERE id = ?', [status, publishAt ?? now, now, id]
		else
			await @db.run 'UPDATE entries SET status = ?, updated_at = ? WHERE id = ?', [status, now, id]
		return

	async remove(id: number): Promise<void>
		await @db.run 'DELETE FROM entries WHERE id = ?', [id]
		return

	async addRevision(entryId: number, title: string, slug: string, data: Record<string, any>, authorId: number | null): Promise<void>
		await @db.run 'INSERT INTO revisions (entry_id, title, slug, data, author_id, created_at) VALUES (?, ?, ?, ?, ?, ?)', [entryId, title, slug, JSON.stringify(data), authorId, Date.now()]
		// Keep the newest 50 per entry.
		await @db.run 'DELETE FROM revisions WHERE entry_id = ? AND id NOT IN (SELECT id FROM revisions WHERE entry_id = ? ORDER BY id DESC LIMIT 50)', [entryId, entryId]
		return

	async revisions(entryId: number): Promise<any[]>
		rows := await @db.all 'SELECT r.*, u.email AS author_email, u.name AS author_name FROM revisions r LEFT JOIN users u ON u.id = r.author_id WHERE r.entry_id = ? ORDER BY r.id DESC LIMIT 50', [entryId]
		rows.map((r: any) => ({ ...r, data: JSON.parse(r.data || '{}') }))

	async revision(entryId: number, id: number): Promise<any>
		r := await @db.get 'SELECT * FROM revisions WHERE entry_id = ? AND id = ?', [entryId, id]
		r ? { ...r, data: JSON.parse(r.data || '{}') } : undefined

	async allIds(): Promise<{ id: number, type: string }[]>
		await @db.all 'SELECT id, type FROM entries', []

	async localeCounts(type: string): Promise<{ locale: string, n: number }[]>
		await @db.all 'SELECT locale, COUNT(*) AS n FROM entries WHERE type = ? GROUP BY locale', [type]
