import { page } from '@human-synthesis/norns/server'
import { redirect } from '@sveltejs/kit'
import { auth, finishLogin, clientIp, isStaff } from '$lib/norns/auth/server/public'
import { loginSchema } from '$lib/norns/auth/shared/schema'
import { settings } from '$lib/norns/settings/server/public'
import { protect } from '$lib/norns/protection/server/public'

export load := page.load
	handler: async ({ user, url, container }) =>
		if user
			throw redirect 303, (isStaff(user) ? '/admin' : '/account')
		site := await settings(container).get 'site'
		{ next: url.searchParams.get('next') ?? '/admin', reset: url.searchParams.has('reset'), siteTitle: site.title }

pageActions := page.actions
	login:
		input: loginSchema
		run: async ({ input, container, event }) =>
			r := await auth(container).login input.email, input.password, { ip: clientIp(event), userAgent: event.request.headers.get('user-agent') ?? '' }
			finishLogin event, r, input.next, input.email

// Honeypot, signed render time and Turnstile (Admin → Form protection).
export actions := protect pageActions, { login: 'auth.staff-login' }
