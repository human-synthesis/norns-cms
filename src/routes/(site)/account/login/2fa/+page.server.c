import { page } from '@human-synthesis/norns/server'
import { redirect } from '@sveltejs/kit'
import { auth, finishLogin, clientIp, MFA_COOKIE } from '$lib/norns/auth/server/public'
import { tFor } from '$lib/norns/i18n/server/public'

// Members' second sign-in step, in the visitor's language. Staff use /login/2fa.
export load := page.load
	handler: ({ event, url }) =>
		unless event.cookies.get MFA_COOKIE
			throw redirect 303, '/account/login'
		{ next: url.searchParams.get('next') ?? '/account', seo: { robots: 'noindex, nofollow' } }

export actions := page.actions
	verify:
		run: async ({ container, event }) =>
			token := event.cookies.get(MFA_COOKIE) ?? ''
			fd := await event.request.formData()
			r := await auth(container).loginMfa token, String(fd.get('code') ?? ''), { ip: clientIp(event), userAgent: event.request.headers.get('user-agent') ?? '' }
			if !r.ok && !r.mfa
				event.cookies.delete MFA_COOKIE, { path: '/' }
				throw redirect 303, '/account/login'
			t := await tFor container, event.locals.locale.language
			if r.mfa
				return { errors: [{ path: [{ key: 'code' }], message: t('twofactor.retry') }] }
			finishLogin event, r, fd.get('next'), '', t
