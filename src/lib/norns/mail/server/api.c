import type { Container } from '@human-synthesis/norns/server'
import { registerEndpoint, paging } from '../../api/server/public'
import { mail } from './public'

export registerMailApi := (app: Container) =>
	registerEndpoint app, {
		method: 'GET', path: '/mail', capability: 'settings.manage', tag: 'Mail'
		summary: 'Sent and logged mail (bodies are never returned: they can hold sign-in links)'
		query: [{ name: 'limit', description: '1–500' }]
		handler: async ({ c, query }) =>
			{ limit } := paging query, 500
			{ data: (await mail(c).log(limit)).map(({ body, ...m }) => m), meta: { provider: mail(c).provider() } }
	}
