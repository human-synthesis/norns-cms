import type { Container } from '@human-synthesis/norns/server'
import { error, redirect } from '@sveltejs/kit'
import type { AuthService } from './service'
import type { User } from './repo'

export type { User }

export COOKIE := 'cms_session'

svc := (c: Container) => c.resolve('auth.service') as AuthService

export auth := (c: Container) => {
	login: (email: string, password: string) => svc(c).login(email, password)
	logout: (sessionId: string) => svc(c).logout(sessionId)
	userForSession: (sessionId: string) => svc(c).userForSession(sessionId)
	createUser: (email: string, name: string, password: string, role?: string) => svc(c).createUser(email, name, password, role)
	hasUsers: () => svc(c).hasUsers()
	can: (user: User | null | undefined, action: string) => svc(c).can(user, action)
}

// SvelteKit handle wired through boot({ extraHandle }): turns the session
// cookie into event.locals.user, and refuses anonymous requests under the
// `protect` prefixes. Runs after contextHandle, so the request container
// already exists.
//
// The guard lives here and not only in admin/+layout.server.c because
// SvelteKit runs form actions BEFORE layout load functions: a layout guard
// alone leaves every `?/action` POST open to anonymous callers.
export sessionHandle := (opts: { protect?: string[] } = {}) =>
	protect := opts.protect ?? []
	async ({ event, resolve }: any) =>
		sid := event.cookies.get COOKIE
		user := sid ? await auth(event.locals.container).userForSession(sid) : undefined
		event.locals.user = user ?? null
		path := event.url.pathname
		if !user && protect.some((p) => path === p || path.startsWith(p + '/'))
			if event.request.method === 'GET' || event.request.method === 'HEAD'
				throw redirect 303, `/login?next=${encodeURIComponent(path)}`
			throw error 403, 'Sign in to do that'
		resolve event
