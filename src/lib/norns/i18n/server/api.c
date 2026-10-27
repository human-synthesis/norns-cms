import type { Container } from '@human-synthesis/norns/server'
import { registerEndpoint, apiError, need } from '../../api/server/public'
import { i18n } from './public'
import { audit } from '../../auth/server/public'

fail := (r: any) => apiError 400, r.error, { field: r.field }

export registerI18nApi := (app: Container) =>
	registerEndpoint app, {
		method: 'GET', path: '/localisations', capability: 'content.view', tag: 'Localisation'
		summary: 'Localisations with their languages, hosting and URLs'
		handler: ({ c }) => i18n(c).map()
	}
	registerEndpoint app, {
		method: 'POST', path: '/localisations', capability: 'i18n.manage', tag: 'Localisation', created: true
		summary: 'Create a localisation'
		body: { description: 'code, name, mode (path | subdomain | domain), prefix (path), host (subdomain / domain), languages [{ code, segment? }], defaultLanguage, prefixDefault, countries, contentFallback, isPrimary, enabled', example: { code: 'ch', name: 'Switzerland', mode: 'path', prefix: 'ch', languages: [{ code: 'de-CH', segment: 'de' }, { code: 'fr-CH', segment: 'fr' }], defaultLanguage: 'de-CH', prefixDefault: true, countries: ['CH'] } }
		handler: async ({ c, body, user }) =>
			r := await i18n(c).saveMarket null, body
			throw fail(r) unless r.ok
			await audit(c).log user, 'i18n.localisation_save', String(body.code), { via: 'api' }
			await i18n(c).market r.id
	}
	registerEndpoint app, {
		method: 'PATCH', path: '/localisations/:id', capability: 'i18n.manage', tag: 'Localisation'
		summary: 'Change a localisation (fields given are merged)'
		body: { description: 'Any localisation field', example: { prefixDefault: true } }
		handler: async ({ c, params, body, user }) =>
			current := need((await i18n(c).market(Number(params.id))), 'Localisation not found')
			r := await i18n(c).saveMarket current.id, { ...current, ...body }
			throw fail(r) unless r.ok
			await audit(c).log user, 'i18n.localisation_save', current.code, { via: 'api' }
			await i18n(c).market current.id
	}
	registerEndpoint app, {
		method: 'DELETE', path: '/localisations/:id', capability: 'i18n.manage', tag: 'Localisation'
		summary: 'Delete a localisation (not the primary one)'
		handler: async ({ c, params, user }) =>
			r := await i18n(c).deleteMarket Number(params.id)
			throw fail(r) unless r.ok
			await audit(c).log user, 'i18n.localisation_delete', `#${params.id}`, { via: 'api' }
			{ deleted: Number(params.id) }
	}
	registerEndpoint app, {
		method: 'GET', path: '/languages', capability: 'content.view', tag: 'Localisation'
		summary: 'Languages, with their fallback, direction and URL words'
		handler: ({ c }) => i18n(c).allLanguages()
	}
	registerEndpoint app, {
		method: 'PUT', path: '/languages/:code', capability: 'i18n.manage', tag: 'Localisation'
		summary: 'Add or change a language'
		body: { description: 'name, native, dir (ltr | rtl), fallback, paths { article, authors, category, tag }, enabled', example: { name: 'German', native: 'Deutsch', paths: { article: 'artikel', authors: 'autoren', category: 'kategorie', tag: 'schlagwort' } } }
		handler: async ({ c, params, body, user }) =>
			r := await i18n(c).saveLanguage { ...body, code: params.code }
			throw fail(r) unless r.ok
			await audit(c).log user, 'i18n.language_save', params.code, { via: 'api' }
			(await i18n(c).allLanguages()).find((l) => l.code === params.code)
	}
	registerEndpoint app, {
		method: 'DELETE', path: '/languages/:code', capability: 'i18n.manage', tag: 'Localisation'
		summary: 'Delete an unused language'
		handler: async ({ c, params, user }) =>
			r := await i18n(c).deleteLanguage params.code
			throw fail(r) unless r.ok
			await audit(c).log user, 'i18n.language_delete', params.code, { via: 'api' }
			{ deleted: params.code }
	}
	registerEndpoint app, {
		method: 'GET', path: '/texts/:code', capability: 'content.view', tag: 'Localisation'
		summary: 'Interface texts of a language: English, built-in, override and the value used'
		query: [{ name: 'missing', description: '1: only keys without a translation of their own' }]
		handler: async ({ c, params, query }) =>
			rows := await i18n(c).stringTable params.code
			query.missing === '1' ? rows.filter((r) => !r.override && !r.builtIn) : rows
	}
	registerEndpoint app, {
		method: 'PATCH', path: '/texts/:code', capability: 'i18n.manage', tag: 'Localisation'
		summary: 'Set interface texts (an empty value removes the override)'
		body: { description: 'Values by key', example: { 'home.latest': 'Neu im Blog' } }
		handler: async ({ c, params, body, user }) =>
			n := await i18n(c).setStrings params.code, body
			await audit(c).log user, 'i18n.texts_save', params.code, { count: n, via: 'api' }
			{ updated: n }
	}
