import { page } from '@human-synthesis/norns/server'
import { consent } from '$lib/norns/consent/server/public'
import { settings } from '$lib/norns/settings/server/public'
import { requireCap, can, audit } from '$lib/norns/auth/server/public'

export load := page.load
	handler: async ({ container, user }) =>
		requireCap user, 'consent.view'
		{
			stats: await consent(container).stats(30)
			config: await settings(container).get('consent')
			scripts: await consent(container).scripts()
			recent: await consent(container).records({ limit: 10 })
			canManage: can(user, 'consent.manage')
		}

export actions := page.actions
	bump:
		run: async ({ container, user }) =>
			requireCap user, 'consent.manage'
			v := await consent(container).bumpVersion user
			await audit(container).log user, 'consent.version', `v${v}`, {}
			{ saved: `Policy version is now ${v}. Every visitor will be asked again.` }
