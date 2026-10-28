import { page } from '@human-synthesis/norns/server'
import { redirect } from '@sveltejs/kit'
import { auth, finishLogin, clientIp, isStaff } from '$lib/norns/auth/server/public'
import { loginSchema } from '$lib/norns/auth/shared/schema'
import { tFor } from '$lib/norns/i18n/server/public'
import { protect } from '$lib/norns/protection/server/public'

export load := page.load
	handler: ({ user, url }) =>
		if user
			throw redirect 303, (isStaff(user) ? '/admin' : '/account')
		{ next: url.searchParams.get('next') ?? '/account', verified: url.searchParams.has('verified'), reset: url.searchParams.has('reset'), seo: { robots: 'noindex, follow' } }

pageActions := page.actions
	login:
		input: loginSchema
		run: async ({ input, container, event }) =>
			r := await auth(container).login input.email, input.password, { ip: clientIp(event), userAgent: event.request.headers.get('user-agent') ?? '' }
			finishLogin event, r, input.next, input.email, await tFor(container, event.locals.locale.language)
	resend:
		run: async ({ container, event }) =>
			email := String((await event.request.formData()).get('email') ?? '')
			await auth(container).resendVerification email, clientIp(event)
			{ resent: true }

// Honeypot, signed render time and Turnstile (Admin → Form protection).
export actions := protect pageActions, { login: 'auth.member-login', resend: 'auth.member-resend' }
