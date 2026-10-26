export class ApiKeysRepo
	db: any
	constructor(@db: any)

	parse(r: any)
		r ? { ...r, capabilities: JSON.parse(r.capabilities || '[]') } : undefined

	async insert(k: { name: string, prefix: string, hash: string, userId: number, capabilities: string[], expiresAt: number | null }): Promise<number>
		r := await @db.run 'INSERT INTO api_keys (name, prefix, hash, user_id, capabilities, expires_at, created_at) VALUES (?, ?, ?, ?, ?, ?, ?)', [k.name, k.prefix, k.hash, k.userId, JSON.stringify(k.capabilities), k.expiresAt, Date.now()]
		r.lastInsertRowid

	async byHash(hash: string)
		@parse await @db.get('SELECT * FROM api_keys WHERE hash = ?', [hash])

	async get(id: number)
		@parse await @db.get('SELECT k.*, u.email AS user_email, u.name AS user_name FROM api_keys k JOIN users u ON u.id = k.user_id WHERE k.id = ?', [id])

	async list(userId: number | null = null): Promise<any[]>
		sql := 'SELECT k.id, k.name, k.prefix, k.user_id, k.capabilities, k.expires_at, k.last_used_at, k.revoked_at, k.created_at, u.email AS user_email, u.name AS user_name FROM api_keys k JOIN users u ON u.id = k.user_id'
		rows := userId ? await @db.all(`${sql} WHERE k.user_id = ? ORDER BY k.id DESC`, [userId]) : await @db.all(`${sql} ORDER BY k.id DESC`, [])
		rows.map((r: any) => @parse(r))

	async touch(id: number): Promise<void>
		await @db.run 'UPDATE api_keys SET last_used_at = ? WHERE id = ?', [Date.now(), id]
		return

	async revoke(id: number): Promise<void>
		await @db.run 'UPDATE api_keys SET revoked_at = ? WHERE id = ? AND revoked_at IS NULL', [Date.now(), id]
		return
