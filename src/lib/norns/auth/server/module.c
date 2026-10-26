import { getScope } from '@human-synthesis/norns/server'
import type { Container } from '@human-synthesis/norns/server'
import { sqliteDb, d1Db, resolveDbPath } from './db.js'
import { UsersRepo } from './repo'
import { AuthService } from './service'
import { registerCapabilities } from './public'
import { registerAdminNav, registerOptions } from '../../content/server/public'
import { registerAuthApi } from './api'

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
	app.bind 'auth.service', (c: Container) => new AuthService c.resolve('auth.repo'), c

	registerCapabilities app, [
		{ id: 'admin.access', label: 'Sign in to the admin', group: 'Admin' }
		{ id: 'users.view', label: 'See users', group: 'Users' }
		{ id: 'users.manage', label: 'Invite, edit, suspend and delete users', group: 'Users' }
		{ id: 'roles.manage', label: 'Create and edit roles', group: 'Users' }
		{ id: 'audit.view', label: 'Read the audit log', group: 'Admin' }
		{ id: 'settings.manage', label: 'Change site settings', group: 'Admin' }
		{ id: 'maintenance.run', label: 'Run maintenance tasks', group: 'Admin' }
		{ id: 'account.access', label: 'Use the member account area', group: 'Members' }
	]
	registerAdminNav app, { id: 'users', label: 'Users', icon: 'lucide:users', href: '/admin/users', section: 'People', order: 50, capability: 'users.view' }
	registerAdminNav app, { id: 'roles', label: 'Roles', icon: 'lucide:key-round', href: '/admin/roles', section: 'People', order: 51, capability: 'roles.manage' }
	registerAdminNav app, { id: 'audit', label: 'Audit log', icon: 'lucide:scroll-text', href: '/admin/audit', section: 'Site', order: 95, capability: 'audit.view' }
	registerOptions app, 'users:staff', (c: Container) => (c.resolve('auth.service') as AuthService).staffOptions()
	registerAuthApi app
