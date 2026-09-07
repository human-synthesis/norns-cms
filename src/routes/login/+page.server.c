import { page } from '@human-synthesis/norns/server'
import { fail, redirect } from '@sveltejs/kit'
import { auth, COOKIE } from '$lib/norns/auth/server/public'
import { loginSchema } from '$lib/norns/auth/shared/schema'

export load := page.load
	handler: ({ user, url }) =>
		if user
			throw redirect 303, '/admin'
		{ next: url.searchParams.get('next') ?? '/admin' }

export actions := page.actions
	login:
		input: loginSchema
		run: async ({ input, container, event }) =>
			session := await auth(container).login input.email, input.password
			unless session
				return fail 400, {
					errors: [{ path: [{ key: 'password' }], message: 'Invalid email or password' }],
					values: { email: input.email },
				}
			event.cookies.set COOKIE, session.id, {
				path: '/',
				httpOnly: true,
				sameSite: 'lax',
				secure: event.url.protocol === 'https:',
				maxAge: 60 * 60 * 24 * 30,
			}
			next := input.next || '/admin'
			throw redirect 303, (next.startsWith('/') && !next.startsWith('//')) ? next : '/admin'
