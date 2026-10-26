import { page } from '@human-synthesis/norns/server'
import { seo } from '$lib/norns/seo/server/public'
import { requireCap } from '$lib/norns/auth/server/public'
import { siteOrigin } from '$lib/norns/settings/server/public'

export load := page.load
	handler: async ({ container, user }) =>
		requireCap user, 'seo.manage'
		{ report: await seo(container).report(), notFound: await seo(container).notFound(), origin: siteOrigin() }

export actions := page.actions
	clear404:
		run: async ({ container, event, user }) =>
			requireCap user, 'seo.manage'
			path := String((await event.request.formData()).get('path') ?? '')
			await seo(container).clearNotFound(path || null)
			{ cleared: true }
