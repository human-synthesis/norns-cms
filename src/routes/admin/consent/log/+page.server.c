import { page } from '@human-synthesis/norns/server'
import { consent } from '$lib/norns/consent/server/public'
import { requireCap } from '$lib/norns/auth/server/public'

export load := page.load
	handler: async ({ container, url, user }) =>
		requireCap user, 'consent.view'
		id := (url.searchParams.get('id') ?? '').trim().toLowerCase()
		action := url.searchParams.get('action') ?? ''
		{ records: await consent(container).records({ consentId: id || undefined, action: action || undefined, limit: 200 }), id, action }
