import { page } from '@human-synthesis/norns/server'
import { redirect } from '@sveltejs/kit'
import { auth, COOKIE } from '$lib/norns/auth/server/public'
import { cms } from '$lib/norns/content/server/public'

export load := page.load
	handler: ({ container }) =>
		counts: cms(container).counts()
		recent: cms(container).recent(8)

export actions := page.actions
	logout:
		run: ({ container, event }) =>
			sid := event.cookies.get COOKIE
			if sid
				auth(container).logout sid
			event.cookies.delete COOKIE, { path: '/' }
			throw redirect 303, '/login'
