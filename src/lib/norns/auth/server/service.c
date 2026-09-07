import { randomBytes } from 'node:crypto'
import type { UsersRepo, User } from './repo'
import { hashPassword, verifyPassword } from './password.js'

SESSION_MS := 30 * 24 * 60 * 60 * 1000

export class AuthService
	repo: UsersRepo
	constructor(@repo: UsersRepo)

	async login(email: string, password: string): Promise<{ id: string, user: User } | null>
		row := await @repo.byEmail email.trim().toLowerCase()
		return null unless row
		return null unless verifyPassword(password, row.password_hash)
		id := randomBytes(32).toString('hex')
		await @repo.createSession id, row.id, Date.now() + SESSION_MS
		{ id, user: { id: row.id, email: row.email, name: row.name, role: row.role } }

	async logout(sessionId: string): Promise<void>
		await @repo.deleteSession sessionId
		return

	async userForSession(sessionId: string): Promise<User | undefined>
		await @repo.userForSession sessionId

	async createUser(email: string, name: string, password: string, role = 'editor'): Promise<number>
		await @repo.create email.trim().toLowerCase(), name.trim(), hashPassword(password), role

	async hasUsers(): Promise<boolean>
		(await @repo.count()) > 0

	// Coarse permissions for now: admins can do everything, everyone else can
	// do everything except manage users.
	can(user: User | null | undefined, action: string): boolean
		return false unless user
		return true if user.role === 'admin'
		action !== 'manage-users'
