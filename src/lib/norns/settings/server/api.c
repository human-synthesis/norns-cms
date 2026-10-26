import type { Container } from '@human-synthesis/norns/server'
import { registerEndpoint, apiError, validationError, need, allow } from '../../api/server/public'
import { settings } from './public'
import { runMaintenance } from './maintenance'
import { can, audit } from '../../auth/server/public'
import { cms } from '../../content/server/public'

// A group names its own capability (seo.manage, consent.manage) or falls
// back to settings.manage.
groupFor := (c: Container, id: string, user: any) =>
	g := need settings(c).group(id), `Unknown settings group "${id}"`
	allow can(user, g.capability ?? 'settings.manage'), `This key lacks "${g.capability ?? 'settings.manage'}"`
	g

describe := (g: any) => ({ id: g.id, label: g.label, description: g.description ?? '', capability: g.capability ?? 'settings.manage', fields: g.fields })

export registerSettingsApi := (app: Container) =>
	registerEndpoint app, {
		method: 'GET', path: '/settings', capability: null, tag: 'Settings'
		summary: 'Settings groups this key may manage, with their fields and values'
		handler: async ({ c, user }) =>
			out := [] as any[]
			for g of settings(c).groups()
				continue unless can(user, g.capability ?? 'settings.manage')
				out.push { ...describe(g), values: await settings(c).get(g.id) }
			out
	}
	registerEndpoint app, {
		method: 'GET', path: '/settings/:group', capability: null, tag: 'Settings'
		summary: 'One settings group'
		handler: async ({ c, params, user }) =>
			g := groupFor c, params.group, user
			{ ...describe(g), values: await settings(c).get(g.id) }
	}
	registerEndpoint app, {
		method: 'PATCH', path: '/settings/:group', capability: null, tag: 'Settings'
		summary: 'Change settings: given values are merged into the current ones and validated'
		body: { description: 'Values by field name', example: { title: 'My site', registration: 'invite' } }
		handler: async ({ c, params, body, user }) =>
			g := groupFor c, params.group, user
			unknown := Object.keys(body).filter((k) => !g.fields.some((f: any) => f.name === k))
			throw apiError(400, `Unknown settings: ${unknown.join(', ')}`) if unknown.length
			r := await settings(c).save g.id, { ...(await settings(c).get g.id), ...body }, user.id
			throw validationError(r.issues) unless r.ok
			await audit(c).log user, 'settings.save', g.id, { via: 'api', fields: Object.keys(body) }
			r.values
	}
	registerEndpoint app, {
		method: 'POST', path: '/maintenance/purge', capability: 'maintenance.run', tag: 'Maintenance'
		summary: 'Purge expired data now (also runs nightly)'
		handler: async ({ c, user }) =>
			result := await runMaintenance c
			await audit(c).log user, 'maintenance.purge', '', { via: 'api' }
			result
	}
	registerEndpoint app, {
		method: 'POST', path: '/maintenance/rerender', capability: 'maintenance.run', tag: 'Maintenance'
		summary: 'Rebuild the cached HTML of every entry'
		handler: async ({ c, user }) =>
			n := await cms(c).rerender()
			await audit(c).log user, 'maintenance.rerender', `${n} entries`, { via: 'api' }
			{ rerendered: n }
	}
