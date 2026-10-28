import { page } from '@human-synthesis/norns/server'
import { fail, redirect } from '@sveltejs/kit'
import { auth } from '$lib/norns/auth/server/public'
import { protect } from '$lib/norns/protection/server/public'

export load := page.load
	handler: async ({ container, params }) =>
		found := await auth(container).peekToken params.token, 'reset'
		{ valid: !!found, email: found?.user.email ?? '' }

pageActions := page.actions
	reset:
		run: async ({ container, event }) =>
			fd := await event.request.formData()
			password := String(fd.get('password') ?? '')
			return fail(400, { errors: [{ path: [{ key: 'confirm' }], message: 'The passwords do not match' }] }) unless password === String(fd.get('confirm') ?? '')
			r := await auth(container).resetPassword event.params.token, password
			return fail(400, { errors: [{ path: [{ key: r.field ?? 'password' }], message: r.error }] }) unless r.ok
			throw redirect 303, (r.user.kind === 'staff' ? '/login?reset=1' : '/account/login?reset=1')

// Honeypot, signed render time and Turnstile (Admin → Form protection).
export actions := protect pageActions, { reset: 'auth.reset' }
