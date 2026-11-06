import type { Container } from '@human-synthesis/norns/server'
import { registerEndpoint, apiError } from '../../api/server/public'
import { seo } from './public'
import { audit } from '../../auth/server/public'
import { cms } from '../../content/server/public'

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
		method: 'GET', path: '/seo/indexing', capability: 'seo.manage', tag: 'SEO'
		summary: 'Search engine notifications: IndexNow and Google Search Console status, queue and the latest submissions'
		handler: ({ c }) => seo(c).indexingStatus()
	}
	registerEndpoint app, {
		method: 'PUT', path: '/seo/indexing/rules', capability: 'seo.manage', tag: 'SEO'
		summary: 'IndexNow on/off and, per content type, which changes are sent automatically (new, updated, removed); left-out values stay'
		body: { description: 'enabled, comments (send a page when a comment on it is approved), auto: { <type>: { new, updated, removed } }', example: { enabled: true, comments: false, auto: { article: { new: true, updated: true, removed: true }, page: { new: true, updated: false } } } }
		handler: async ({ c, body, user }) =>
			r := await seo(c).saveIndexingRules body
			await audit(c).log user, 'seo.indexing_rules', '', { via: 'api' }
			r
	}
	registerEndpoint app, {
		method: 'POST', path: '/seo/indexing/entries/:id', capability: 'content.publish', tag: 'SEO'
		summary: 'Send one published entry to IndexNow now (all its localised URLs), whatever the automatic rules say'
		handler: async ({ c, params, user }) =>
			entry := await cms(c).byId Number(params.id)
			ct := entry ? cms(c).getType(entry.type) : null
			throw apiError(404, 'Entry not found') unless entry && ct
			r := await seo(c).submitEntry ct, entry
			throw apiError(400, r.reason) if r.reason
			await audit(c).log user, 'seo.indexing_submit', `${ct.id}#${entry.id}`, { via: 'api', urls: r.sent }
			r
	}
	registerEndpoint app, {
		method: 'POST', path: '/seo/indexing/urls', capability: 'seo.manage', tag: 'SEO'
		summary: 'Send chosen URLs to IndexNow now; URLs on other hosts are skipped'
		body: { description: 'urls: a list, or one string with a URL per line', example: { urls: ['https://example.com/pricing'] } }
		handler: async ({ c, body, user }) =>
			r := await seo(c).submitUrls body.urls ?? []
			throw apiError(400, r.reason) if r.reason
			await audit(c).log user, 'seo.indexing_submit', `${r.sent} URLs`, { via: 'api' }
			r
	}
	registerEndpoint app, {
		method: 'POST', path: '/seo/indexing/submit-all', capability: 'seo.manage', tag: 'SEO'
		summary: 'Send every public, indexable URL to IndexNow now (after a launch or a domain move)'
		handler: async ({ c, user }) =>
			r := await seo(c).submitAll()
			throw apiError(400, r.reason) if r.reason
			await audit(c).log user, 'seo.indexing_submit', `${r.sent} URLs`, { via: 'api' }
			r
	}
	registerEndpoint app, {
		method: 'POST', path: '/seo/indexing/google', capability: 'seo.manage', tag: 'SEO'
		summary: 'Resubmit the sitemap of every host to Google Search Console now'
		handler: async ({ c, user }) =>
			r := await seo(c).submitSitemaps()
			throw apiError(400, r.skipped) if r.skipped
			await audit(c).log user, 'seo.indexing_google', '', { via: 'api', ok: r.ok }
			r
	}
	registerEndpoint app, {
		method: 'GET', path: '/seo/not-found/probes', capability: 'seo.manage', tag: 'SEO'
		summary: '404 paths that look like vulnerability scans, grouped under the pattern suggested for Blocked paths'
		handler: ({ c }) => seo(c).probeSuggestions()
	}
	registerEndpoint app, {
		method: 'POST', path: '/seo/not-found/block', capability: 'protection.manage', tag: 'SEO'
		summary: 'Move a path or pattern from the 404 log to Blocked paths (blocks it and clears the 404 rows it covers)'
		body: { description: 'pattern (a path, /folder* or *.ext), note?', example: { pattern: '*.php' } }
		handler: async ({ c, body, user }) =>
			r := await seo(c).blockFromLog String(body.pattern ?? ''), user, String(body.note ?? '')
			throw apiError(400, r.error, { field: 'pattern' }) unless r.ok
			await audit(c).log user, 'protection.block_path', r.pattern, { via: 'api', from: '404', cleared: r.cleared }
			r
	}
	registerEndpoint app, {
		method: 'GET', path: '/seo/report', capability: 'seo.manage', tag: 'SEO'
		summary: 'Entries with SEO warnings'
		handler: ({ c }) => seo(c).report()
	}
