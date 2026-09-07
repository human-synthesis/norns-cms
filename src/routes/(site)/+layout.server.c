import { page } from '@human-synthesis/norns/server'
import { env } from '$env/dynamic/private'
import { cms } from '$lib/norns/content/server/public'

export load := page.load
	handler: ({ container, user }) =>
		site: { title: env.CMS_SITE_TITLE || 'Norns CMS', tagline: env.CMS_SITE_TAGLINE || '' }
		nav: cms(container).nav()
		user: user ? { email: user.email } : null
