import { page } from '@human-synthesis/norns/server'
import { fail, redirect } from '@sveltejs/kit'
import { auth, COOKIE } from '$lib/norns/auth/server/public'
import { consent } from '$lib/norns/consent/server/public'

export load := page.load
	handler: async ({ container, event, user }) =>
		state := await consent(container).state event
		{ state, records: await consent(container).records({ userId: user.id, limit: 20 }), seo: { title: 'Privacy & data', robots: 'noindex, nofollow' } }

export actions := page.actions
	delete:
		run: async ({ container, event, user }) =>
			fd := await event.request.formData()
			return fail(400, { errors: [{ path: [{ key: 'confirm' }], message: 'Type DELETE to confirm' }] }) unless String(fd.get('confirm') ?? '').trim() === 'DELETE'
			r := await auth(container).deleteOwnAccount user, String(fd.get('password') ?? '')
			return fail(400, { errors: [{ path: [{ key: r.field ?? 'password' }], message: r.error }] }) unless r.ok
			event.cookies.delete COOKIE, { path: '/' }
			throw redirect 303, '/?account=deleted'
