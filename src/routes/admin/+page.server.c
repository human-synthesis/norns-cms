import { page } from '@human-synthesis/norns/server'
import { redirect } from '@sveltejs/kit'
import { auth, COOKIE, can } from '$lib/norns/auth/server/public'
import { cms } from '$lib/norns/content/server/public'
import { consent } from '$lib/norns/consent/server/public'
import { seo } from '$lib/norns/seo/server/public'
import { media } from '$lib/norns/media/server/public'

export load := page.load
	handler: async ({ container, user }) =>
		counts := await cms(container).counts()
		review := can(user, 'content.publish') ? await cms(container).list(null, { status: 'in_review', limit: 10 }) : []
		mine := can(user, 'content.edit_any') ? [] : await cms(container).list(null, { authorId: user.id, limit: 8 })
		scheduled := await cms(container).list null, { status: 'scheduled', limit: 5, order: 'published' }
		{
			counts
			recent: await cms(container).recent(8)
			review
			mine
			scheduled
			media: can(user, 'media.view') ? await media(container).count() : null
			users: can(user, 'users.view') ? { staff: await auth(container).countUsers({ kind: 'staff' }), clients: await auth(container).countUsers({ kind: 'client' }) } : null
			consent: can(user, 'consent.view') ? await consent(container).stats(30) : null
			notFound: can(user, 'seo.manage') ? await seo(container).count404() : null
		}

export actions := page.actions
	logout:
		run: async ({ container, event }) =>
			sid := event.cookies.get COOKIE
			if sid
				await auth(container).logout sid
			event.cookies.delete COOKIE, { path: '/' }
			throw redirect 303, '/login'
