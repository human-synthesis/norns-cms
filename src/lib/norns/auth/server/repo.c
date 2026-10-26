export type User =
	id: number
	email: string
	name: string
	role: string
	kind: string
	status: string
	slug: string | null
	bio: string
	avatar_media_id: number | null
	email_verified_at: number | null
	last_login_at: number | null
	totp_enabled: number
	created_at: number

export type UserRow = User & { password_hash: string, totp_secret: string | null, recovery_codes: string }

USER_COLS := 'id, email, name, role, kind, status, slug, bio, avatar_media_id, email_verified_at, last_login_at, totp_enabled, created_at, password_changed_at'

export type UserQuery =
	kind?: string
	role?: string
	status?: string
	q?: string
	limit?: number
	offset?: number

// Hand-written SQL over the async db surface (see db.js). Users, sessions,
// roles, one-time tokens, the audit log and rate-limit counters.
export class UsersRepo
	db: any
	constructor(@db: any)

	// --- users -------------------------------------------------------------

	async byEmail(email: string): Promise<UserRow | undefined>
		await @db.get 'SELECT * FROM users WHERE email = ?', [email]

	async byId(id: number): Promise<UserRow | undefined>
		await @db.get 'SELECT * FROM users WHERE id = ?', [id]

	async bySlug(slug: string): Promise<User | undefined>
		await @db.get `SELECT ${USER_COLS} FROM users WHERE slug = ? AND kind = 'staff' AND status = 'active'`, [slug]

	async byIds(ids: number[]): Promise<User[]>
		return [] unless ids.length
		await @db.all `SELECT ${USER_COLS} FROM users WHERE id IN (${ids.map(=> '?').join(',')})`, ids

	async slugTaken(slug: string, excludeId: number): Promise<boolean>
		!!(await @db.get 'SELECT id FROM users WHERE slug = ? AND id != ?', [slug, excludeId])

	where(q: UserQuery)
		where := [] as string[]
		params := [] as any[]
		if q.kind
			where.push 'kind = ?'
			params.push q.kind
		if q.role
			where.push 'role = ?'
			params.push q.role
		if q.status
			where.push 'status = ?'
			params.push q.status
		else
			where.push "status != 'deleted'"
		if q.q
			where.push '(email LIKE ? OR name LIKE ?)'
			params.push `%${q.q}%`, `%${q.q}%`
		{ sql: where.length ? where.join(' AND ') : '1 = 1', params }

	async list(q: UserQuery = {}): Promise<User[]>
		{ sql, params } := @where q
		await @db.all `SELECT ${USER_COLS} FROM users WHERE ${sql} ORDER BY created_at DESC LIMIT ? OFFSET ?`, [...params, q.limit ?? 50, q.offset ?? 0]

	async countWhere(q: UserQuery = {}): Promise<number>
		{ sql, params } := @where q
		row := await @db.get `SELECT COUNT(*) AS n FROM users WHERE ${sql}`, params
		Number(row?.n ?? 0)

	async count(): Promise<number>
		row := await @db.get 'SELECT COUNT(*) AS n FROM users', []
		Number(row?.n ?? 0)

	async create(input: { email: string, name: string, passwordHash: string, role: string, kind: string, status: string, verified: boolean, slug: string | null }): Promise<number>
		now := Date.now()
		result := await @db.run 'INSERT INTO users (email, name, password_hash, role, kind, status, slug, email_verified_at, password_changed_at, created_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)', [input.email, input.name, input.passwordHash, input.role, input.kind, input.status, input.slug, input.verified ? now : null, now, now]
		result.lastInsertRowid

	async update(id: number, fields: Record<string, any>): Promise<void>
		allowed := ['email', 'name', 'role', 'kind', 'status', 'slug', 'bio', 'avatar_media_id', 'email_verified_at', 'last_login_at', 'totp_secret', 'totp_enabled', 'recovery_codes', 'deleted_at']
		keys := Object.keys(fields).filter((k) => allowed.includes(k))
		return unless keys.length
		await @db.run `UPDATE users SET ${keys.map((k) => `${k} = ?`).join(', ')} WHERE id = ?`, [...keys.map((k) => fields[k]), id]
		return

	async setPassword(id: number, hash: string): Promise<void>
		await @db.run 'UPDATE users SET password_hash = ?, password_changed_at = ? WHERE id = ?', [hash, Date.now(), id]
		return

	async remove(id: number): Promise<void>
		await @db.run 'DELETE FROM users WHERE id = ?', [id]
		return

	async countActiveOwners(): Promise<number>
		row := await @db.get "SELECT COUNT(*) AS n FROM users WHERE role = 'owner' AND status = 'active'", []
		Number(row?.n ?? 0)

	async purgeDeleted(before: number): Promise<number>
		r := await @db.run "DELETE FROM users WHERE status = 'deleted' AND deleted_at < ?", [before]
		r.changes

	// --- sessions ----------------------------------------------------------

	async createSession(id: string, userId: number, expiresAt: number, userAgent: string): Promise<void>
		now := Date.now()
		await @db.run 'INSERT INTO sessions (id, user_id, expires_at, created_at, user_agent, last_seen_at) VALUES (?, ?, ?, ?, ?, ?)', [id, userId, expiresAt, now, userAgent.slice(0, 200), now]
		return

	async sessionUser(id: string): Promise<any>
		await @db.get `SELECT s.id AS session_id, s.expires_at, s.last_seen_at, u.${USER_COLS.split(', ').join(', u.')} FROM sessions s JOIN users u ON u.id = s.user_id WHERE s.id = ? AND s.expires_at > ?`, [id, Date.now()]

	async touchSession(id: string, expiresAt: number): Promise<void>
		await @db.run 'UPDATE sessions SET last_seen_at = ?, expires_at = ? WHERE id = ?', [Date.now(), expiresAt, id]
		return

	async sessions(userId: number): Promise<any[]>
		await @db.all 'SELECT id, user_agent, created_at, last_seen_at, expires_at FROM sessions WHERE user_id = ? AND expires_at > ? ORDER BY last_seen_at DESC', [userId, Date.now()]

	async deleteSession(id: string): Promise<void>
		await @db.run 'DELETE FROM sessions WHERE id = ?', [id]
		return

	async deleteUserSession(userId: number, id: string): Promise<void>
		await @db.run 'DELETE FROM sessions WHERE user_id = ? AND id = ?', [userId, id]
		return

	async deleteSessions(userId: number, exceptId: string | null = null): Promise<void>
		await @db.run 'DELETE FROM sessions WHERE user_id = ? AND id != ?', [userId, exceptId ?? '']
		return

	async purgeSessions(): Promise<number>
		r := await @db.run 'DELETE FROM sessions WHERE expires_at < ?', [Date.now()]
		r.changes

	// --- roles -------------------------------------------------------------

	async roles(kind?: string): Promise<any[]>
		rows := kind ? await @db.all('SELECT * FROM roles WHERE kind = ? ORDER BY builtin DESC, label', [kind]) : await @db.all('SELECT * FROM roles ORDER BY kind DESC, builtin DESC, label', [])
		rows.map((r: any) => ({ ...r, capabilities: JSON.parse(r.capabilities || '[]') }))

	async role(id: string): Promise<any>
		r := await @db.get 'SELECT * FROM roles WHERE id = ?', [id]
		r ? { ...r, capabilities: JSON.parse(r.capabilities || '[]') } : undefined

	async saveRole(role: { id: string, kind: string, label: string, description: string, capabilities: string[] }): Promise<void>
		await @db.run 'INSERT INTO roles (id, kind, label, description, capabilities, builtin, created_at) VALUES (?, ?, ?, ?, ?, 0, ?) ON CONFLICT(id) DO UPDATE SET label = excluded.label, description = excluded.description, capabilities = excluded.capabilities', [role.id, role.kind, role.label, role.description, JSON.stringify(role.capabilities), Date.now()]
		return

	async deleteRole(id: string): Promise<void>
		await @db.run 'DELETE FROM roles WHERE id = ? AND builtin = 0', [id]
		return

	async roleUsage(id: string): Promise<number>
		row := await @db.get "SELECT COUNT(*) AS n FROM users WHERE role = ? AND status != 'deleted'", [id]
		Number(row?.n ?? 0)

	// --- tokens ------------------------------------------------------------

	async createToken(id: string, userId: number, purpose: string, expiresAt: number): Promise<void>
		await @db.run 'DELETE FROM tokens WHERE user_id = ? AND purpose = ? AND used_at IS NULL', [userId, purpose]
		await @db.run 'INSERT INTO tokens (id, user_id, purpose, expires_at, created_at) VALUES (?, ?, ?, ?, ?)', [id, userId, purpose, expiresAt, Date.now()]
		return

	async token(id: string, purpose: string): Promise<any>
		await @db.get 'SELECT * FROM tokens WHERE id = ? AND purpose = ? AND used_at IS NULL AND expires_at > ?', [id, purpose, Date.now()]

	async useToken(id: string): Promise<boolean>
		r := await @db.run 'UPDATE tokens SET used_at = ? WHERE id = ? AND used_at IS NULL', [Date.now(), id]
		r.changes > 0

	async purgeTokens(): Promise<number>
		r := await @db.run 'DELETE FROM tokens WHERE expires_at < ? OR used_at IS NOT NULL', [Date.now() - 24 * 3600 * 1000]
		r.changes

	// --- audit -------------------------------------------------------------

	async audit(actorId: number | null, actor: string, action: string, target: string, meta: Record<string, any>): Promise<void>
		await @db.run 'INSERT INTO audit_log (actor_id, actor, action, target, meta, created_at) VALUES (?, ?, ?, ?, ?, ?)', [actorId, actor, action, target, JSON.stringify(meta ?? {}), Date.now()]
		return

	async auditList(q: { actorId?: number, action?: string, q?: string, limit?: number, offset?: number } = {}): Promise<any[]>
		where := [] as string[]
		params := [] as any[]
		if q.actorId
			where.push 'actor_id = ?'
			params.push q.actorId
		if q.action
			where.push 'action LIKE ?'
			params.push `${q.action}%`
		if q.q
			where.push '(actor LIKE ? OR target LIKE ? OR action LIKE ?)'
			params.push `%${q.q}%`, `%${q.q}%`, `%${q.q}%`
		sql := where.length ? `WHERE ${where.join(' AND ')}` : ''
		rows := await @db.all `SELECT * FROM audit_log ${sql} ORDER BY id DESC LIMIT ? OFFSET ?`, [...params, q.limit ?? 100, q.offset ?? 0]
		rows.map((r: any) => ({ ...r, meta: JSON.parse(r.meta || '{}') }))

	async purgeAudit(before: number): Promise<number>
		r := await @db.run 'DELETE FROM audit_log WHERE created_at < ?', [before]
		r.changes

	// --- rate limits -------------------------------------------------------

	// Fixed window counter. Returns the count after this hit.
	async hit(key: string, windowMs: number): Promise<number>
		now := Date.now()
		row := await @db.get 'SELECT count, window_start FROM rate_limits WHERE key = ?', [key]
		if !row || now - row.window_start > windowMs
			await @db.run 'INSERT INTO rate_limits (key, count, window_start) VALUES (?, 1, ?) ON CONFLICT(key) DO UPDATE SET count = 1, window_start = excluded.window_start', [key, now]
			return 1
		await @db.run 'UPDATE rate_limits SET count = count + 1 WHERE key = ?', [key]
		row.count + 1

	async peekHits(key: string, windowMs: number): Promise<number>
		row := await @db.get 'SELECT count, window_start FROM rate_limits WHERE key = ?', [key]
		return 0 if !row || Date.now() - row.window_start > windowMs
		row.count

	async clearHits(key: string): Promise<void>
		await @db.run 'DELETE FROM rate_limits WHERE key = ?', [key]
		return

	async purgeRateLimits(before: number): Promise<number>
		r := await @db.run 'DELETE FROM rate_limits WHERE window_start < ?', [before]
		r.changes
