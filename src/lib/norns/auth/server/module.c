import type { Container } from '@human-synthesis/norns/server'
import { openSqlite, resolveDbPath } from './sqlite.js'
import { UsersRepo } from './repo'
import { AuthService } from './service'

// auth owns the single `db` binding for the whole app. Every other feature
// resolves 'db' from the container; nobody else opens the file.
export default (app: Container) =>
	app.single 'db', => openSqlite resolveDbPath()
	app.single 'auth.repo', (c: Container) => new UsersRepo c.resolve('db')
	app.single 'auth.service', (c: Container) => new AuthService c.resolve('auth.repo')
