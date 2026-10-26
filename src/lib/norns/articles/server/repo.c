export type Term =
	id: number
	taxonomy: string
	slug: string
	name: string
	description: string
	parent_id: number | null
	sort: number
	created_at: number

export class TermsRepo
	db: any
	constructor(@db: any)

	async list(taxonomy: string): Promise<(Term & { count: number })[]>
		await @db.all "SELECT t.*, (SELECT COUNT(*) FROM entry_terms et JOIN entries e ON e.id = et.entry_id WHERE et.term_id = t.id AND e.status = 'published') AS count FROM terms t WHERE t.taxonomy = ? ORDER BY t.sort, t.name COLLATE NOCASE", [taxonomy]

	async get(id: number): Promise<Term | undefined>
		await @db.get 'SELECT * FROM terms WHERE id = ?', [id]

	async bySlug(taxonomy: string, slug: string): Promise<Term | undefined>
		await @db.get 'SELECT * FROM terms WHERE taxonomy = ? AND slug = ?', [taxonomy, slug]

	async byIds(ids: number[]): Promise<Term[]>
		return [] unless ids.length
		await @db.all `SELECT * FROM terms WHERE id IN (${ids.map(=> '?').join(',')})`, ids

	async insert(t: { taxonomy: string, slug: string, name: string, description?: string, parent_id?: number | null, sort?: number }): Promise<number>
		r := await @db.run 'INSERT INTO terms (taxonomy, slug, name, description, parent_id, sort, created_at) VALUES (?, ?, ?, ?, ?, ?, ?)', [t.taxonomy, t.slug, t.name, t.description ?? '', t.parent_id ?? null, t.sort ?? 0, Date.now()]
		r.lastInsertRowid

	async update(id: number, t: { slug: string, name: string, description: string, parent_id: number | null, sort: number }): Promise<void>
		await @db.run 'UPDATE terms SET slug = ?, name = ?, description = ?, parent_id = ?, sort = ? WHERE id = ?', [t.slug, t.name, t.description, t.parent_id, t.sort, id]
		return

	async remove(id: number): Promise<void>
		await @db.run 'UPDATE terms SET parent_id = NULL WHERE parent_id = ?', [id]
		await @db.run 'DELETE FROM terms WHERE id = ?', [id]
		return

	async slugTaken(taxonomy: string, slug: string, excludeId: number): Promise<boolean>
		!!(await @db.get 'SELECT id FROM terms WHERE taxonomy = ? AND slug = ? AND id != ?', [taxonomy, slug, excludeId])

	async setEntryTerms(entryId: number, taxonomy: string, termIds: number[]): Promise<void>
		await @db.run 'DELETE FROM entry_terms WHERE entry_id = ? AND term_id IN (SELECT id FROM terms WHERE taxonomy = ?)', [entryId, taxonomy]
		for id of termIds
			await @db.run 'INSERT OR IGNORE INTO entry_terms (entry_id, term_id) VALUES (?, ?)', [entryId, id]
		return

	async entryTerms(entryIds: number[]): Promise<any[]>
		return [] unless entryIds.length
		await @db.all `SELECT et.entry_id, t.* FROM entry_terms et JOIN terms t ON t.id = et.term_id WHERE et.entry_id IN (${entryIds.map(=> '?').join(',')}) ORDER BY t.name COLLATE NOCASE`, entryIds

	async entriesOf(termId: number): Promise<number[]>
		rows := await @db.all 'SELECT entry_id FROM entry_terms WHERE term_id = ?', [termId]
		rows.map((r: any) => r.entry_id)
