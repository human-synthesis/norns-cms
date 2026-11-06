import { page } from '@human-synthesis/norns/server'
import { blocked, countryFilter, protection, cloudflare } from '$lib/norns/protection/server/public'
import { requireCap, can } from '$lib/norns/auth/server/public'
import { settings } from '$lib/norns/settings/server/public'

export load := page.load
	handler: async ({ container, event, user }) =>
		requireCap user, 'protection.manage'
		paths := await blocked(container).list()
		{ filter, you, stats } := await countryFilter(container).status(event)
		config := await protection(container).config()
		{
			paths: { total: paths.length, on: paths.filter((p) => p.enabled).length, hits: paths.reduce(((n, p) => n + p.hits), 0), top: [...paths].sort((a, b) => b.hits - a.hits).slice(0, 5).filter((p) => p.hits) }
			countries: { filter, you, refused: stats.reduce(((n, s) => n + s.hits), 0), top: stats.slice(0, 5) }
			turnstile: !!config?.enabled
			cloudflare: await cloudflare(container).status()
			csp: can(user, 'settings.manage') ? (await settings(container).get('security')).csp : null
		}
