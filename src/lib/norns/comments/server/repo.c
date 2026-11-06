export class CommentsRepo
	db: any
	constructor(@db: any)

	async insert(row: Record<string, any>): Promise<number>
		now := Date.now()
		r := await @db.run 'INSERT INTO comments (entry_id, parent_id, depth, user_id, staff, author_name, author_email, body, status, locale, ip_hash, created_at, updated_at, approved_at, approved_by) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)', [row.entryId, row.parentId ?? null, row.depth ?? 0, row.userId ?? null, row.staff ? 1 : 0, row.name, row.email ?? '', row.body, row.status, row.locale ?? '', row.ipHash ?? '', now, now, (row.status === 'approved' ? now : null), row.approvedBy ?? null]
		r.lastInsertRowid

	async get(id: number): Promise<any>
		await @db.get 'SELECT * FROM comments WHERE id = ?', [id]

	async forEntry(entryId: number, statuses: string[]): Promise<any[]>
		await @db.all `SELECT * FROM comments WHERE entry_id = ? AND status IN (${statuses.map(=> '?').join(', ')}) ORDER BY created_at, id`, [entryId, ...statuses]

	async countApproved(entryIds: number[]): Promise<Record<number, number>>
		return {} unless entryIds.length
		rows := await @db.all `SELECT entry_id, COUNT(*) AS n FROM comments WHERE status = 'approved' AND entry_id IN (${entryIds.map(=> '?').join(', ')}) GROUP BY entry_id`, entryIds
		Object.fromEntries rows.map((r: any) => [r.entry_id, Number(r.n)])

	where(opts: Record<string, any>)
		where := [] as string[]
		params := [] as any[]
		if opts.status
			where.push 'status = ?'
			params.push opts.status
		if opts.entryId
			where.push 'entry_id = ?'
			params.push opts.entryId
		if opts.q
			where.push '(body LIKE ? OR author_name LIKE ? OR author_email LIKE ?)'
			params.push `%${opts.q}%`, `%${opts.q}%`, `%${opts.q}%`
		{ sql: where.length ? where.join(' AND ') : '1 = 1', params }

	async list(opts: Record<string, any> = {}): Promise<any[]>
		{ sql, params } := @where opts
		await @db.all `SELECT * FROM comments WHERE ${sql} ORDER BY created_at DESC, id DESC LIMIT ? OFFSET ?`, [...params, opts.limit ?? 50, opts.offset ?? 0]

	async count(opts: Record<string, any> = {}): Promise<number>
		{ sql, params } := @where opts
		row := await @db.get `SELECT COUNT(*) AS n FROM comments WHERE ${sql}`, params
		Number(row?.n ?? 0)

	async counts(): Promise<Record<string, number>>
		rows := await @db.all 'SELECT status, COUNT(*) AS n FROM comments GROUP BY status', []
		Object.fromEntries rows.map((r: any) => [r.status, Number(r.n)])

	async setStatus(id: number, status: string, by: number | null): Promise<void>
		now := Date.now()
		if status === 'approved'
			await @db.run 'UPDATE comments SET status = ?, updated_at = ?, approved_at = COALESCE(approved_at, ?), approved_by = COALESCE(approved_by, ?) WHERE id = ?', [status, now, now, by, id]
		else
			await @db.run 'UPDATE comments SET status = ?, updated_at = ? WHERE id = ?', [status, now, id]
		return

	async setBody(id: number, body: string): Promise<void>
		await @db.run 'UPDATE comments SET body = ?, updated_at = ? WHERE id = ?', [body, Date.now(), id]
		return

	// Deleting a comment keeps its replies: they move up to its parent.
	async remove(id: number): Promise<void>
		row := await @get id
		return unless row
		await @db.run 'UPDATE comments SET parent_id = ?, depth = MAX(depth - 1, 0) WHERE parent_id = ?', [row.parent_id, id]
		await @db.run 'DELETE FROM comments WHERE id = ?', [id]
		return

	async removeForEntry(entryId: number): Promise<void>
		await @db.run 'DELETE FROM comments WHERE entry_id = ?', [entryId]
		return

	// A deleted member's comments stay, without their link or email.
	async anonymize(userId: number, name: string): Promise<void>
		await @db.run "UPDATE comments SET user_id = NULL, author_email = '', author_name = ? WHERE user_id = ?", [name, userId]
		return

	async byUser(userId: number): Promise<any[]>
		await @db.all 'SELECT id, entry_id, body, status, created_at FROM comments WHERE user_id = ? ORDER BY id', [userId]

	async purge(before: number): Promise<number>
		r := await @db.run "DELETE FROM comments WHERE status IN ('spam', 'trash') AND updated_at < ?", [before]
		r.changes

	async userIds(): Promise<number[]>
		rows := await @db.all 'SELECT DISTINCT user_id FROM comments WHERE user_id IS NOT NULL', []
		rows.map((r: any) => Number(r.user_id))
