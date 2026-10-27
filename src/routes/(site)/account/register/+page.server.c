import { page } from '@human-synthesis/norns/server'
import { fail, redirect, error } from '@sveltejs/kit'
import { auth, clientIp } from '$lib/norns/auth/server/public'
import { settings } from '$lib/norns/settings/server/public'
import { tFor } from '$lib/norns/i18n/server/public'

export load := page.load
	handler: async ({ container, user }) =>
		throw redirect(303, '/account') if user
		site := await settings(container).get 'site'
		unless site.registration === 'open'
			throw error 404, 'Registration is closed'
		{ seo: { robots: 'noindex, follow' } }

export actions := page.actions
	register:
		run: async ({ container, event }) =>
			fd := await event.request.formData()
			values := { email: String(fd.get('email') ?? ''), name: String(fd.get('name') ?? '') }
			t := await tFor container, event.locals.locale.language
			return fail(400, { errors: [{ path: [{ key: 'terms' }], message: t('register.acceptTerms') }], values }) unless fd.get('terms')
			r := await auth(container).register { ...values, password: String(fd.get('password') ?? ''), locale: event.locals.locale.language }, clientIp(event)
			return fail(400, { errors: [{ path: [{ key: r.field ?? 'email' }], message: r.error }], values }) unless r.ok
			{ registered: values.email }
