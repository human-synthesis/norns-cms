import { page } from '@human-synthesis/norns/server'
import { redirect } from '@sveltejs/kit'
import { auth, finishLogin, clientIp, MFA_COOKIE } from '$lib/norns/auth/server/public'
import { protect } from '$lib/norns/protection/server/public'

export load := page.load
	handler: ({ event, url }) =>
		unless event.cookies.get MFA_COOKIE
			throw redirect 303, '/login'
		{ next: url.searchParams.get('next') ?? '/admin' }

pageActions := page.actions
	verify:
		run: async ({ container, event }) =>
			token := event.cookies.get(MFA_COOKIE) ?? ''
			fd := await event.request.formData()
			r := await auth(container).loginMfa token, String(fd.get('code') ?? ''), { ip: clientIp(event), userAgent: event.request.headers.get('user-agent') ?? '' }
			if !r.ok && !r.mfa
				event.cookies.delete MFA_COOKIE, { path: '/' }
				throw redirect 303, '/login'
			if r.mfa
				return { errors: [{ path: [{ key: 'code' }], message: 'That code is not right. Try the next one, or a recovery code.' }] }
			finishLogin event, r, fd.get('next')

// Honeypot, signed render time and Turnstile (Admin → Form protection).
export actions := protect pageActions, { verify: 'twofactor.staff-step' }
