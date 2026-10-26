import type { Container } from '@human-synthesis/norns/server'
import { registerEndpoint, apiError } from '../../api/server/public'
import { seo } from './public'
import { audit } from '../../auth/server/public'

export registerSeoApi := (app: Container) =>
	registerEndpoint app, {
		method: 'GET', path: '/seo/redirects', capability: 'seo.manage', tag: 'SEO'
		summary: 'Redirects (manual, automatic and imported)'
		query: [{ name: 'q', description: 'Search paths' }]
		handler: ({ c, query }) => seo(c).redirects(query.q ?? '')
	}
	registerEndpoint app, {
		method: 'POST', path: '/seo/redirects', capability: 'seo.manage', tag: 'SEO', created: true
		summary: 'Add or replace a redirect (loops and self-redirects are refused)'
		body: { description: 'from, to, status (301 | 302 | 307 | 308 | 410)', example: { from: '/old', to: '/new', status: 301 } }
		handler: async ({ c, body, user }) =>
			r := await seo(c).saveRedirect null, body
			throw apiError(400, r.error, { field: r.field }) unless r.ok
			await audit(c).log user, 'seo.redirect', String(body.from), { to: body.to, status: body.status, via: 'api' }
			{ saved: true }
	}
	registerEndpoint app, {
		method: 'PATCH', path: '/seo/redirects/:id', capability: 'seo.manage', tag: 'SEO'
		summary: 'Change a redirect'
		body: { description: 'from, to, status', example: { to: '/newer' } }
		handler: async ({ c, params, body, user }) =>
			current := (await seo(c).redirects()).find((r) => r.id === Number(params.id))
			throw apiError(404, 'Redirect not found') unless current
			r := await seo(c).saveRedirect current.id, { from: body.from ?? current.from_path, to: body.to ?? current.to_path, status: body.status ?? current.status }
			throw apiError(400, r.error, { field: r.field }) unless r.ok
			await audit(c).log user, 'seo.redirect', current.from_path, { via: 'api' }
			{ saved: true }
	}
	registerEndpoint app, {
		method: 'DELETE', path: '/seo/redirects/:id', capability: 'seo.manage', tag: 'SEO'
		summary: 'Remove a redirect'
		handler: async ({ c, params, user }) =>
			await seo(c).removeRedirect Number(params.id)
			await audit(c).log user, 'seo.redirect_delete', `redirect#${params.id}`, { via: 'api' }
			{ deleted: Number(params.id) }
	}
	registerEndpoint app, {
		method: 'POST', path: '/seo/redirects/import', capability: 'seo.manage', tag: 'SEO'
		summary: 'Import redirects from CSV lines: from,to[,status]'
		body: { description: 'csv', example: { csv: '/old,/new\n/gone,,410' } }
		handler: async ({ c, body, user }) =>
			r := await seo(c).importRedirects String(body.csv ?? '')
			await audit(c).log user, 'seo.redirect_import', `${r.imported} rows`, { via: 'api' }
			r
	}
	registerEndpoint app, {
		method: 'GET', path: '/seo/not-found', capability: 'seo.manage', tag: 'SEO'
		summary: 'Paths visitors hit that do not exist, most hits first'
		handler: ({ c }) => seo(c).notFound()
	}
	registerEndpoint app, {
		method: 'DELETE', path: '/seo/not-found', capability: 'seo.manage', tag: 'SEO'
		summary: 'Clear the 404 log, or one path with ?path='
		query: [{ name: 'path', description: 'One path to clear' }]
		handler: async ({ c, query }) =>
			await seo(c).clearNotFound(query.path || null)
			{ cleared: true }
	}
	registerEndpoint app, {
		method: 'GET', path: '/seo/report', capability: 'seo.manage', tag: 'SEO'
		summary: 'Entries with SEO warnings'
		handler: ({ c }) => seo(c).report()
	}
