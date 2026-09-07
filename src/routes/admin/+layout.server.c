import { page } from '@human-synthesis/norns/server'
import { redirect } from '@sveltejs/kit'
import { cms } from '$lib/norns/content/server/public'

// Every /admin route requires a session. Login lives at /login so this
// guard can stay unconditional.
export load := page.load
	handler: ({ user, url, container }) =>
		unless user
			throw redirect 303, `/login?next=${encodeURIComponent(url.pathname)}`
		{ user, nav: cms(container).adminNav() }
