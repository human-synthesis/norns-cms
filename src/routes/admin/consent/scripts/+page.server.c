import { page } from '@human-synthesis/norns/server'
import { consent } from '$lib/norns/consent/server/public'
import { requireCap } from '$lib/norns/auth/server/public'

export load := page.load
	handler: async ({ container, user }) =>
		requireCap user, 'consent.manage'
		{ scripts: await consent(container).scripts(), providers: consent(container).providers() }
