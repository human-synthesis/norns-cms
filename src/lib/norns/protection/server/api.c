import type { Container } from '@human-synthesis/norns/server'
import { registerEndpoint, apiError } from '../../api/server/public'
import { protection, blocked, countryFilter, countryOf, cloudflare } from './public'
import { audit } from '../../auth/server/public'

export registerProtectionApi := (app: Container) =>
	registerEndpoint app, {
		method: 'GET', path: '/protection', capability: 'protection.manage', tag: 'Form protection'
		summary: 'Turnstile configuration (never the secret key) and whether it is active'
		handler: ({ c }) => protection(c).config()
	}
	registerEndpoint app, {
		method: 'PATCH', path: '/protection', capability: 'protection.manage', tag: 'Form protection'
		summary: 'Set the Turnstile keys and switches; a new secret key is checked with Cloudflare and stored encrypted'
		body: { description: 'enabled?, siteKey?, secretKey? (write-only), clearSecret?, appearance? (always | interaction-only)', example: { enabled: true, siteKey: '0x4AAAAAAA…', secretKey: '0x4AAAAAAA…', appearance: 'interaction-only' } }
		handler: async ({ c, body, user }) =>
			r := await protection(c).saveConfig body ?? {}, user, { via: 'api' }
			throw apiError(400, r.error, { field: r.field }) unless r.ok
			{ ...(await protection(c).config()), warning: r.warning || undefined }
	}
	registerEndpoint app, {
		method: 'GET', path: '/protection/forms', capability: 'protection.manage', tag: 'Form protection'
		summary: 'Every registered form with its Turnstile and honeypot switches and the last 30 days of outcomes'
		handler: ({ c }) => protection(c).forms()
	}
	registerEndpoint app, {
		method: 'PATCH', path: '/protection/forms/:id', capability: 'protection.manage', tag: 'Form protection'
		summary: 'Change one form’s protection'
		body: { description: 'turnstile? (default | on | off), honeypot? (boolean)', example: { turnstile: 'on', honeypot: true } }
		handler: async ({ c, params, body, user }) =>
			r := await protection(c).saveForm params.id, body ?? {}, user, { via: 'api' }
			throw apiError((r.error === 'Form not found' ? 404 : 400), r.error, { field: r.field }) unless r.ok
			(await protection(c).forms()).find (f) => f.id === params.id
	}
	registerEndpoint app, {
		method: 'GET', path: '/protection/paths', capability: 'protection.manage', tag: 'Protection'
		summary: 'Blocked paths: patterns answered with an empty 403 before anything else runs, with hit counts'
		handler: ({ c }) => blocked(c).list()
	}
	registerEndpoint app, {
		method: 'POST', path: '/protection/paths', capability: 'protection.manage', tag: 'Protection', created: true
		summary: 'Block a path or pattern (/wp-login.php, /wp-admin*, *.php); patterns that would block the site itself are refused'
		body: { description: 'pattern, note?', example: { pattern: '/wp-login.php', note: 'WordPress login probes' } }
		handler: async ({ c, body, user }) =>
			r := await blocked(c).add { pattern: String(body.pattern ?? ''), note: body.note }, user
			throw apiError(400, r.error, { field: 'pattern' }) unless r.ok
			await audit(c).log user, 'protection.block_path', r.pattern, { via: 'api' }
			r.row
	}
	registerEndpoint app, {
		method: 'PATCH', path: '/protection/paths/:id', capability: 'protection.manage', tag: 'Protection'
		summary: 'Switch a blocked path on or off, or change its note'
		body: { description: 'enabled, note', example: { enabled: false } }
		handler: async ({ c, params, body, user }) =>
			row := await blocked(c).update Number(params.id), { enabled: body.enabled, note: body.note }
			throw apiError(404, 'Blocked path not found') unless row
			await audit(c).log user, 'protection.block_path_update', row.pattern, { via: 'api', enabled: body.enabled }
			row
	}
	registerEndpoint app, {
		method: 'DELETE', path: '/protection/paths/:id', capability: 'protection.manage', tag: 'Protection'
		summary: 'Stop blocking a path (remove it from the list)'
		handler: async ({ c, params, user }) =>
			await blocked(c).remove Number(params.id)
			await audit(c).log user, 'protection.unblock_path', `path#${params.id}`, { via: 'api' }
			{ deleted: Number(params.id) }
	}
	registerEndpoint app, {
		method: 'POST', path: '/protection/paths/defaults', capability: 'protection.manage', tag: 'Protection'
		summary: 'Add the starting list of scanner paths again (those missing)'
		handler: async ({ c, user }) =>
			added := await blocked(c).restoreDefaults()
			await audit(c).log user, 'protection.block_defaults', `${added} patterns`, { via: 'api' }
			{ added }
	}
	registerEndpoint app, {
		method: 'GET', path: '/protection/paths/check', capability: 'protection.manage', tag: 'Protection'
		summary: 'Is a path blocked, by which pattern, and what pattern would cover it'
		query: [{ name: 'path', description: 'A path such as /wp-login.php' }]
		handler: ({ c, query }) => blocked(c).check String(query.path ?? '')
	}
	registerEndpoint app, {
		method: 'GET', path: '/protection/countries', capability: 'protection.manage', tag: 'Protection'
		summary: 'Country filter: mode (off | allow | block), countries, unknown-country handling, whether the admin is filtered, refusals per country'
		handler: async ({ c, event }) =>
			{ filter, you, stats } := await countryFilter(c).status(event)
			{ filter, you, stats }
	}
	registerEndpoint app, {
		method: 'PUT', path: '/protection/countries', capability: 'protection.manage', tag: 'Protection'
		summary: 'Set the country filter; refused when it would lock the caller out of the admin'
		body: { description: 'mode (off | allow | block), countries (ISO codes, T1 = Tor), unknown (allow | block), admin (filter the admin, sign-in and API too)', example: { mode: 'allow', countries: ['RO', 'DE', 'AT'], unknown: 'allow', admin: false } }
		handler: async ({ c, body, user, event }) =>
			r := await countryFilter(c).save body, { user, country: countryOf(event) }
			throw apiError(400, r.error) unless r.ok
			await audit(c).log user, 'protection.countries', r.filter.mode, { via: 'api', countries: r.filter.countries.length, admin: r.filter.admin }
			r.filter
	}
	registerEndpoint app, {
		method: 'DELETE', path: '/protection/countries/stats', capability: 'protection.manage', tag: 'Protection'
		summary: 'Reset the per-country refusal counts'
		handler: async ({ c, user }) =>
			await countryFilter(c).clearStats()
			await audit(c).log user, 'protection.countries_stats_clear', '', { via: 'api' }
			{ cleared: true }
	}
	registerEndpoint app, {
		method: 'GET', path: '/protection/cloudflare', capability: 'protection.manage', tag: 'Protection'
		summary: 'Cloudflare firewall: whether a token is saved (never the token), the zone, what is mirrored, the last sync and its rules'
		handler: ({ c }) => cloudflare(c).status()
	}
	registerEndpoint app, {
		method: 'PUT', path: '/protection/cloudflare', capability: 'protection.manage', tag: 'Protection'
		summary: 'Save the Cloudflare API token (Zone WAF: Edit, Zone: Read), zone and switches; a new token is checked first, then the rules are synced'
		body: { description: 'token?, clearToken?, zoneId? (empty: found from the site host), paths, countries, auto', example: { token: 'cf-api-token', paths: true, countries: true, auto: true } }
		handler: async ({ c, body, user }) =>
			r := await cloudflare(c).save body, user
			throw apiError(400, r.error) unless r.ok
			await audit(c).log user, 'protection.cloudflare', '', { via: 'api', token: !!body.token, synced: r.synced?.ok ?? null }
			{ ...r.status, synced: r.synced }
	}
	registerEndpoint app, {
		method: 'POST', path: '/protection/cloudflare/sync', capability: 'protection.manage', tag: 'Protection'
		summary: 'Write the blocked paths and country filter to the zone as WAF custom rules now'
		handler: async ({ c, user }) =>
			r := await cloudflare(c).sync()
			await audit(c).log user, 'protection.cloudflare_sync', '', { via: 'api', ok: r.ok }
			throw apiError(502, r.error) unless r.ok
			r
	}
	registerEndpoint app, {
		method: 'DELETE', path: '/protection/cloudflare/rules', capability: 'protection.manage', tag: 'Protection'
		summary: "Remove the CMS's rules from the zone (other rules stay)"
		handler: async ({ c, user }) =>
			r := await cloudflare(c).remove()
			await audit(c).log user, 'protection.cloudflare_remove', '', { via: 'api', ok: r.ok }
			throw apiError(502, r.error) unless r.ok
			r
	}
	registerEndpoint app, {
		method: 'GET', path: '/protection/cloudflare/preview', capability: 'protection.manage', tag: 'Protection'
		summary: 'The rules the CMS would write now (expressions), without calling Cloudflare'
		handler: ({ c }) => cloudflare(c).preview()
	}
