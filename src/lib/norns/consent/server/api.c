import type { Container } from '@human-synthesis/norns/server'
import { registerEndpoint, apiError, paging } from '../../api/server/public'
import { consent } from './public'
import { can, audit } from '../../auth/server/public'

// API shape → the admin form's raw shape (config_<name> fields).
toRaw := (body: any, current: any = null) =>
	raw := {
		provider: body.provider ?? current?.provider
		name: body.name ?? current?.name
		category: body.category ?? current?.category
		placement: body.placement ?? current?.placement ?? 'head'
		code: body.code ?? current?.code ?? ''
		cookies: body.cookies ?? current?.cookies ?? ''
		enabled: body.enabled ?? current?.enabled ?? true
		sort: body.sort ?? current?.sort ?? 0
	} as Record<string, any>
	for [k, v] of Object.entries({ ...(current?.config ?? {}), ...(body.config ?? {}) })
		raw[`config_${k}`] = v
	raw

guardCustom := (user: any, provider: string, current: any) =>
	if (provider === 'custom' || current?.provider === 'custom') && !can(user, 'consent.scripts_custom')
		throw apiError 403, 'Custom code needs the consent.scripts_custom capability'

export registerConsentApi := (app: Container) =>
	registerEndpoint app, {
		method: 'GET', path: '/consent/stats', capability: 'consent.view', tag: 'Consent'
		summary: 'Consent choices and acceptance rates'
		query: [{ name: 'days', description: 'Window, default 30' }]
		handler: ({ c, query }) => consent(c).stats(Number(query.days) || 30)
	}
	registerEndpoint app, {
		method: 'GET', path: '/consent/records', capability: 'consent.view', tag: 'Consent'
		summary: 'Consent records (proof of consent; no IP addresses)'
		query: [{ name: 'consentId', description: 'One visitor\'s records' }, { name: 'action', description: 'accept_all | reject_all | custom | withdraw | acknowledge' }, { name: 'limit', description: '1–1000' }, { name: 'offset', description: 'Default 0' }]
		handler: async ({ c, query }) =>
			{ limit, offset } := paging query, 1000
			{ data: await consent(c).records({ consentId: query.consentId || undefined, action: query.action || undefined, limit, offset }), meta: { limit, offset } }
	}
	registerEndpoint app, {
		method: 'GET', path: '/consent/providers', capability: 'consent.view', tag: 'Consent'
		summary: 'Script presets and their config fields'
		handler: ({ c }) => consent(c).providers()
	}
	registerEndpoint app, {
		method: 'GET', path: '/consent/scripts', capability: 'consent.view', tag: 'Consent'
		summary: 'Scripts injected after consent'
		handler: ({ c }) => consent(c).scripts()
	}
	registerEndpoint app, {
		method: 'POST', path: '/consent/scripts', capability: 'consent.manage', tag: 'Consent', created: true
		summary: 'Add a script (preset ids are validated; custom code needs consent.scripts_custom)'
		body: { description: 'provider, name, category, placement, config, code (custom), cookies, enabled', example: { provider: 'ga4', category: 'analytics', config: { measurementId: 'G-XXXXXXXXXX' }, enabled: true } }
		handler: async ({ c, body, user }) =>
			guardCustom user, body.provider, null
			r := await consent(c).saveScript null, toRaw(body), user
			throw apiError(400, r.error, { field: r.field }) unless r.ok
			await audit(c).log user, 'consent.script_save', r.name, { provider: r.provider, via: 'api' }
			await consent(c).script r.id
	}
	registerEndpoint app, {
		method: 'PATCH', path: '/consent/scripts/:id', capability: 'consent.manage', tag: 'Consent'
		summary: 'Change a script (fields given are merged)'
		body: { description: 'Any script field', example: { enabled: false } }
		handler: async ({ c, params, body, user }) =>
			current := await consent(c).script Number(params.id)
			throw apiError(404, 'Script not found') unless current
			guardCustom user, body.provider ?? current.provider, current
			r := await consent(c).saveScript current.id, toRaw(body, current), user
			throw apiError(400, r.error, { field: r.field }) unless r.ok
			await audit(c).log user, 'consent.script_save', r.name, { provider: r.provider, via: 'api' }
			await consent(c).script current.id
	}
	registerEndpoint app, {
		method: 'DELETE', path: '/consent/scripts/:id', capability: 'consent.manage', tag: 'Consent'
		summary: 'Remove a script'
		handler: async ({ c, params, user }) =>
			current := await consent(c).script Number(params.id)
			throw apiError(404, 'Script not found') unless current
			guardCustom user, current.provider, current
			await consent(c).removeScript current.id
			await audit(c).log user, 'consent.script_delete', current.name, { via: 'api' }
			{ deleted: current.id }
	}
	registerEndpoint app, {
		method: 'POST', path: '/consent/version', capability: 'consent.manage', tag: 'Consent'
		summary: 'Raise the policy version: every visitor is asked again'
		handler: async ({ c, user }) =>
			v := await consent(c).bumpVersion user
			await audit(c).log user, 'consent.version', `v${v}`, { via: 'api' }
			{ policyVersion: v }
	}
