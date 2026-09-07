export type User =
	id: number
	email: string
	name: string
	role: string

type UserRow = User & { password_hash: string, created_at: number }

// Hand-written SQL over the async db surface (see db.js).
export class UsersRepo
	db: any
	constructor(@db: any)

	async byEmail(email: string): Promise<UserRow | undefined>
		await @db.get 'SELECT * FROM users WHERE email = ?', [email]

	async create(email: string, name: string, passwordHash: string, role: string): Promise<number>
		result := await @db.run 'INSERT INTO users (email, name, password_hash, role, created_at) VALUES (?, ?, ?, ?, ?)', [email, name, passwordHash, role, Date.now()]
		result.lastInsertRowid

	async count(): Promise<number>
		row := await @db.get 'SELECT COUNT(*) AS n FROM users', []
		Number(row?.n ?? 0)

	async createSession(id: string, userId: number, expiresAt: number): Promise<void>
		await @db.run 'INSERT INTO sessions (id, user_id, expires_at, created_at) VALUES (?, ?, ?, ?)', [id, userId, expiresAt, Date.now()]
		return

	async userForSession(id: string): Promise<User | undefined>
		await @db.get 'SELECT u.id, u.email, u.name, u.role FROM sessions s JOIN users u ON u.id = s.user_id WHERE s.id = ? AND s.expires_at > ?', [id, Date.now()]

	async deleteSession(id: string): Promise<void>
		await @db.run 'DELETE FROM sessions WHERE id = ?', [id]
		return
