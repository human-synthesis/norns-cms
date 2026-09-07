import { getScope } from '@human-synthesis/norns/server'
import type { Container } from '@human-synthesis/norns/server'
import { sqliteDb, d1Db, resolveDbPath } from './db.js'
import { UsersRepo } from './repo'
import { AuthService } from './service'

// auth owns the `db` token for the whole app; nobody else opens a database.
//
// `db` is transient: per resolve it hands out D1 when the current request
// carries a binding (event.platform.env.DB — wrangler in production, the
// adapter-cloudflare platform proxy in `bun run dev`), otherwise the
// process-wide SQLite file (`bun test`, a Node/Bun server without D1).
// Repos and services are transient too, so they close over the right db.
export default (app: Container) =>
	app.single 'db.sqlite', => sqliteDb resolveDbPath()
	app.bind 'db', (c: Container) =>
		binding := getScope()?.event?.platform?.env?.DB
		binding ? d1Db(binding) : c.resolve('db.sqlite')
	app.bind 'auth.repo', (c: Container) => new UsersRepo c.resolve('db')
	app.bind 'auth.service', (c: Container) => new AuthService c.resolve('auth.repo')
