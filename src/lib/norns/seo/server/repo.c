export class SeoRepo
	db: any
	constructor(@db: any)

	async redirectFor(path: string): Promise<any>
		await @db.get 'SELECT * FROM redirects WHERE from_path = ?', [path]

	async hit(id: number): Promise<void>
		await @db.run 'UPDATE redirects SET hits = hits + 1, last_hit_at = ? WHERE id = ?', [Date.now(), id]
		return

	async redirects(q = '', limit = 200): Promise<any[]>
		if q
			await @db.all 'SELECT * FROM redirects WHERE from_path LIKE ? OR to_path LIKE ? ORDER BY id DESC LIMIT ?', [`%${q}%`, `%${q}%`, limit]
		else
			await @db.all 'SELECT * FROM redirects ORDER BY id DESC LIMIT ?', [limit]

	async redirect(id: number): Promise<any>
		await @db.get 'SELECT * FROM redirects WHERE id = ?', [id]

	async upsertRedirect(from: string, to: string, status: number, source: string): Promise<void>
		await @db.run 'INSERT INTO redirects (from_path, to_path, status, source, created_at) VALUES (?, ?, ?, ?, ?) ON CONFLICT(from_path) DO UPDATE SET to_path = excluded.to_path, status = excluded.status, source = excluded.source', [from, to, status, source, Date.now()]
		return

	async updateRedirect(id: number, from: string, to: string, status: number): Promise<void>
		await @db.run 'UPDATE redirects SET from_path = ?, to_path = ?, status = ? WHERE id = ?', [from, to, status, id]
		return

	async removeRedirect(id: number): Promise<void>
		await @db.run 'DELETE FROM redirects WHERE id = ?', [id]
		return

	async removeFrom(path: string): Promise<void>
		await @db.run 'DELETE FROM redirects WHERE from_path = ?', [path]
		return

	// Point every redirect that ends at `oldTo` at `newTo` (no chains).
	async retarget(oldTo: string, newTo: string): Promise<void>
		await @db.run 'UPDATE redirects SET to_path = ? WHERE to_path = ?', [newTo, oldTo]
		return

	async log404(path: string, referrer: string): Promise<void>
		now := Date.now()
		await @db.run 'INSERT INTO not_found (path, hits, referrer, first_seen, last_seen) VALUES (?, 1, ?, ?, ?) ON CONFLICT(path) DO UPDATE SET hits = hits + 1, last_seen = excluded.last_seen, referrer = CASE WHEN excluded.referrer != \'\' THEN excluded.referrer ELSE not_found.referrer END', [path, referrer, now, now]
		return

	async notFound(limit = 200): Promise<any[]>
		await @db.all 'SELECT * FROM not_found ORDER BY hits DESC, last_seen DESC LIMIT ?', [limit]

	async clearNotFound(path: string | null): Promise<void>
		if path
			await @db.run 'DELETE FROM not_found WHERE path = ?', [path]
		else
			await @db.run 'DELETE FROM not_found', []
		return

	async count404(): Promise<number>
		row := await @db.get 'SELECT COUNT(*) AS n FROM not_found', []
		Number(row?.n ?? 0)

	async purge404(before: number): Promise<number>
		r := await @db.run 'DELETE FROM not_found WHERE last_seen < ?', [before]
		r.changes
