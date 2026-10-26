import type { Container } from '@human-synthesis/norns/server'
import { registerEndpoint, apiError, paging, need } from '../../api/server/public'
import { auth, audit, passwordProblem } from './public'
import { cms } from '../../content/server/public'

userOf := async (c: Container, id: string) =>
	u := await auth(c).getUser Number(id)
	throw apiError(404, 'User not found') unless u && u.status !== 'deleted'
	u

fail := (r: any) => apiError 400, r.error, { field: r.field }

export registerAuthApi := (app: Container) =>
	registerEndpoint app, {
		method: 'GET', path: '/users', capability: 'users.view', tag: 'Users'
		summary: 'List users'
		query: [{ name: 'kind', description: 'staff | client' }, { name: 'role', description: 'Role id' }, { name: 'status', description: 'active | invited | suspended' }, { name: 'q', description: 'Search name and email' }, { name: 'limit', description: '1–500' }, { name: 'offset', description: 'Default 0' }]
		handler: async ({ c, query }) =>
			{ limit, offset } := paging query, 500
			q := { kind: query.kind || undefined, role: query.role || undefined, status: query.status || undefined, q: query.q || undefined }
			{ data: await auth(c).listUsers({ ...q, limit, offset }), meta: { total: await auth(c).countUsers(q), limit, offset } }
	}
	registerEndpoint app, {
		method: 'GET', path: '/users/:id', capability: 'users.view', tag: 'Users'
		summary: 'One user with their active sessions'
		handler: async ({ c, params }) =>
			u := await userOf c, params.id
			{ ...u, sessions: (await auth(c).sessions u.id).map((s) => ({ id: s.id.slice(0, 8), user_agent: s.user_agent, last_seen_at: s.last_seen_at, created_at: s.created_at })) }
	}
	registerEndpoint app, {
		method: 'POST', path: '/users', capability: 'users.manage', tag: 'Users', created: true
		summary: 'Invite a user, or create one with a password'
		description: 'Without password an invitation is emailed (with the log mail provider its link is returned). Only owners create owners.'
		body: { description: 'email, name, role, password?', example: { email: 'writer@example.com', name: 'Wren', role: 'author' } }
		handler: async ({ c, body, user }) =>
			unless body.password
				r := await auth(c).invite { email: String(body.email ?? ''), name: String(body.name ?? ''), role: String(body.role ?? '') }, user
				throw fail(r) unless r.ok
				return { ...(await auth(c).getUser r.id), inviteLink: r.link, mailed: r.mailed }
			email := String(body.email ?? '').trim().toLowerCase()
			throw apiError(400, 'Enter a valid email', { field: 'email' }) unless /^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(email)
			throw apiError(409, 'An account with that email exists', { field: 'email' }) if await auth(c).userByEmail(email)
			role := await auth(c).role String(body.role ?? '')
			throw apiError(400, 'Unknown role', { field: 'role' }) unless role
			throw apiError(403, 'Only an owner can create owners') if role.id === 'owner' && user.role !== 'owner'
			problem := passwordProblem body.password, email
			throw apiError(400, problem, { field: 'password' }) if problem
			id := await auth(c).createUser email, String(body.name ?? ''), String(body.password), role.id, { kind: role.kind, verified: true }
			await audit(c).log user, 'user.create', email, { role: role.id, via: 'api' }
			await auth(c).getUser id
	}
	registerEndpoint app, {
		method: 'PATCH', path: '/users/:id', capability: 'users.manage', tag: 'Users'
		summary: 'Update name, email, role, status (active | suspended), bio, slug, avatar'
		description: 'Owner rules apply: only owners change owners, the last owner stays, nobody changes their own role or status.'
		body: { description: 'Any of name, email, role, status, bio, slug, avatar (media id)', example: { role: 'editor' } }
		handler: async ({ c, params, body, user }) =>
			u := await userOf c, params.id
			r := await auth(c).updateUser u.id, body, user
			throw fail(r) unless r.ok
			await auth(c).getUser u.id
	}
	registerEndpoint app, {
		method: 'DELETE', path: '/users/:id', capability: 'users.manage', tag: 'Users'
		summary: 'Delete a user; ?reassign=<user id> gives their entries to someone else'
		query: [{ name: 'reassign', description: 'User id to receive their entries' }]
		handler: async ({ c, params, query, user }) =>
			u := await userOf c, params.id
			r := await auth(c).deleteUser u.id, user
			throw fail(r) unless r.ok
			await cms(c).reassignAuthor u.id, (Number(query.reassign) || null)
			{ deleted: u.id }
	}
	registerEndpoint app, {
		method: 'POST', path: '/users/:id/password-reset', capability: 'users.manage', tag: 'Users'
		summary: 'Email the user a password reset link'
		handler: async ({ c, params, user }) =>
			u := await userOf c, params.id
			await auth(c).requestReset u.email, 'api'
			await audit(c).log user, 'user.reset_sent', u.email, { via: 'api' }
			{ sent: true }
	}
	registerEndpoint app, {
		method: 'POST', path: '/users/:id/sessions/revoke', capability: 'users.manage', tag: 'Users'
		summary: 'Sign the user out everywhere'
		handler: async ({ c, params, user }) =>
			u := await userOf c, params.id
			await auth(c).revokeAll u.id
			await audit(c).log user, 'user.sessions_revoked', u.email, { via: 'api' }
			{ revoked: true }
	}
	registerEndpoint app, {
		method: 'POST', path: '/users/:id/2fa/reset', capability: 'users.manage', tag: 'Users'
		summary: 'Remove the user\'s two-factor sign-in'
		handler: async ({ c, params, user }) =>
			r := await auth(c).resetTotp Number(params.id), user
			throw fail(r) unless r.ok
			{ reset: true }
	}
	registerEndpoint app, {
		method: 'GET', path: '/roles', capability: 'users.view', tag: 'Roles'
		summary: 'Roles with their capabilities'
		query: [{ name: 'kind', description: 'staff | client' }]
		handler: ({ c, query }) => auth(c).roles(query.kind || undefined)
	}
	registerEndpoint app, {
		method: 'GET', path: '/capabilities', capability: 'users.view', tag: 'Roles'
		summary: 'Every capability the installed features declare'
		handler: ({ c }) => auth(c).capabilities()
	}
	registerEndpoint app, {
		method: 'POST', path: '/roles', capability: 'roles.manage', tag: 'Roles', created: true
		summary: 'Create a custom role'
		body: { description: 'label, description, kind (staff | client), capabilities', example: { label: 'SEO specialist', kind: 'staff', capabilities: ['seo.manage', 'content.view'] } }
		handler: async ({ c, body, user }) =>
			r := await auth(c).saveRole { kind: body.kind ?? 'staff', label: body.label, description: body.description ?? '', capabilities: body.capabilities ?? [] }, user
			throw fail(r) unless r.ok
			await auth(c).role r.id
	}
	registerEndpoint app, {
		method: 'PATCH', path: '/roles/:id', capability: 'roles.manage', tag: 'Roles'
		summary: 'Update a custom role (built-in roles are fixed)'
		body: { description: 'label, description, capabilities', example: { capabilities: ['seo.manage'] } }
		handler: async ({ c, params, body, user }) =>
			current := need(await auth(c).role(params.id), 'Role not found')
			r := await auth(c).saveRole { id: current.id, kind: current.kind, label: body.label ?? current.label, description: body.description ?? current.description, capabilities: body.capabilities ?? current.capabilities }, user
			throw fail(r) unless r.ok
			await auth(c).role current.id
	}
	registerEndpoint app, {
		method: 'DELETE', path: '/roles/:id', capability: 'roles.manage', tag: 'Roles'
		summary: 'Delete a custom role nobody has'
		handler: async ({ c, params, user }) =>
			r := await auth(c).deleteRole params.id, user
			throw fail(r) unless r.ok
			{ deleted: params.id }
	}
	registerEndpoint app, {
		method: 'GET', path: '/audit', capability: 'audit.view', tag: 'Audit'
		summary: 'The audit log, newest first'
		query: [{ name: 'q', description: 'Search people, targets, actions' }, { name: 'action', description: 'Action prefix, e.g. user. or content.' }, { name: 'limit', description: '1–500' }, { name: 'offset', description: 'Default 0' }]
		handler: async ({ c, query }) =>
			{ limit, offset } := paging query, 500
			{ data: await audit(c).list({ q: query.q || undefined, action: query.action || undefined, limit, offset }), meta: { limit, offset } }
	}
