export type User =
	id: number
	email: string
	name: string
	role: string

type UserRow = User & { password_hash: string, created_at: number }

export class UsersRepo
	db: any
	constructor(@db: any)

	// bun:sqlite returns null for "no row"; normalise to undefined.
	byEmail(email: string): UserRow | undefined
		(@db.prepare('SELECT * FROM users WHERE email = ?').get(email) as UserRow | null) ?? undefined

	create(email: string, name: string, passwordHash: string, role: string): number
		result := @db
			.prepare 'INSERT INTO users (email, name, password_hash, role, created_at) VALUES (?, ?, ?, ?, ?)'
			.run email, name, passwordHash, role, Date.now()
		Number(result.lastInsertRowid)

	count(): number
		row := @db.prepare('SELECT COUNT(*) AS n FROM users').get() as { n: number }
		row.n

	createSession(id: string, userId: number, expiresAt: number): void
		@db
			.prepare 'INSERT INTO sessions (id, user_id, expires_at, created_at) VALUES (?, ?, ?, ?)'
			.run id, userId, expiresAt, Date.now()

	userForSession(id: string): User | undefined
		row := @db
			.prepare 'SELECT u.id, u.email, u.name, u.role FROM sessions s JOIN users u ON u.id = s.user_id WHERE s.id = ? AND s.expires_at > ?'
			.get id, Date.now()
		(row as User | null) ?? undefined

	deleteSession(id: string): void
		@db.prepare('DELETE FROM sessions WHERE id = ?').run(id)
