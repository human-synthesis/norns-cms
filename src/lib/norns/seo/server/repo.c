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

	// --- search engine notifications -----------------------------------------

	async queue(urls: string[]): Promise<void>
		now := Date.now()
		for url of urls
			await @db.run 'INSERT OR IGNORE INTO indexing_queue (url, queued_at) VALUES (?, ?)', [url, now]
		return

	// Removes and returns the oldest queued URLs.
	async takeQueue(limit: number): Promise<string[]>
		rows := await @db.all 'SELECT url FROM indexing_queue ORDER BY queued_at, url LIMIT ?', [limit]
		urls := rows.map((r: any) => r.url as string)
		for let i = 0; i < urls.length; i += 50
			chunk := urls.slice(i, i + 50)
			await @db.run `DELETE FROM indexing_queue WHERE url IN (${chunk.map(=> '?').join(', ')})`, chunk
		urls

	async queued(): Promise<number>
		row := await @db.get 'SELECT COUNT(*) AS n FROM indexing_queue', []
		Number(row?.n ?? 0)

	async logIndexing(engine: string, target: string, urls: number, ok: boolean, status: number | null, message: string): Promise<void>
		await @db.run 'INSERT INTO indexing_log (engine, target, urls, ok, status, message, created_at) VALUES (?, ?, ?, ?, ?, ?, ?)', [engine, target, urls, ok ? 1 : 0, status, message, Date.now()]
		return

	async indexingLog(limit = 30): Promise<any[]>
		await @db.all 'SELECT * FROM indexing_log ORDER BY id DESC LIMIT ?', [limit]

	async purgeIndexingLog(before: number): Promise<number>
		r := await @db.run 'DELETE FROM indexing_log WHERE created_at < ?', [before]
		r.changes

	async state(key: string): Promise<string | null>
		row := await @db.get 'SELECT value FROM seo_state WHERE key = ?', [key]
		row?.value ?? null

	async setState(key: string, value: string): Promise<void>
		await @db.run 'INSERT INTO seo_state (key, value) VALUES (?, ?) ON CONFLICT(key) DO UPDATE SET value = excluded.value', [key, value]
		return

	// Sets a value only when none is stored; returns the stored one.
	async initState(key: string, value: string): Promise<string>
		await @db.run 'INSERT OR IGNORE INTO seo_state (key, value) VALUES (?, ?)', [key, value]
		(await @state key) ?? value
