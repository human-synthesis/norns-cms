import { page } from '@human-synthesis/norns/server'
import { fail, redirect } from '@sveltejs/kit'
import { auth, setSessionCookie } from '$lib/norns/auth/server/public'
import { settings } from '$lib/norns/settings/server/public'

export load := page.load
	handler: async ({ container, params }) =>
		found := await auth(container).peekToken params.token, 'invite'
		site := await settings(container).get 'site'
		{ valid: !!found, email: found?.user.email ?? '', name: found?.user.name ?? '', siteTitle: site.title }

export actions := page.actions
	accept:
		run: async ({ container, event }) =>
			fd := await event.request.formData()
			password := String(fd.get('password') ?? '')
			name := String(fd.get('name') ?? '')
			return fail(400, { errors: [{ path: [{ key: 'confirm' }], message: 'The passwords do not match' }], values: { name } }) unless password === String(fd.get('confirm') ?? '')
			r := await auth(container).acceptInvite event.params.token, name, password
			return fail(400, { errors: [{ path: [{ key: r.field ?? 'password' }], message: r.error }], values: { name } }) unless r.ok
			// Accepting proves the address; sign them straight in.
			session := await auth(container).startSession r.user, event.request.headers.get('user-agent') ?? ''
			setSessionCookie event, session
			throw redirect 303, (r.user.kind === 'staff' ? '/admin/profile' : '/account')
