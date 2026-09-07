import { page } from '@human-synthesis/norns/server'
import { redirect } from '@sveltejs/kit'
import { auth, COOKIE } from '$lib/norns/auth/server/public'
import { cms } from '$lib/norns/content/server/public'

export load := page.load
	handler: async ({ container }) =>
		counts: await cms(container).counts()
		recent: await cms(container).recent(8)

export actions := page.actions
	logout:
		run: async ({ container, event }) =>
			sid := event.cookies.get COOKIE
			if sid
				await auth(container).logout sid
			event.cookies.delete COOKIE, { path: '/' }
			throw redirect 303, '/login'
