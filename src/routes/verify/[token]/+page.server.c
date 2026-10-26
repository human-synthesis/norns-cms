import { page } from '@human-synthesis/norns/server'
import { redirect } from '@sveltejs/kit'
import { auth } from '$lib/norns/auth/server/public'

// Following the emailed link confirms the address; GET is enough because
// the token is single-use and only ever proves control of the inbox.
export load := page.load
	handler: async ({ container, params }) =>
		r := await auth(container).verifyEmail params.token
		throw redirect(303, '/account/login?verified=1') if r.ok
		{ ok: false }
