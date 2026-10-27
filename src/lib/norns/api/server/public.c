import type { Container } from '@human-synthesis/norns/server'
import type { ApiKeysService } from './service'
import { apiError, ApiError, match, openapi } from './router.js'
import { settings, siteOrigin } from '../../settings/server/public'
import { auth, can } from '../../auth/server/public'

export { apiError, ApiError }

svc := (c: Container) => c.resolve('api.keys') as ApiKeysService

export apiKeys := (c: Container) => {
	create: (input: any, creator: any) => svc(c).create(input, creator)
	list: (userId?: number | null) => svc(c).list(userId)
	get: (id: number) => svc(c).get(id)
	revoke: (id: number, actor: any) => svc(c).revoke(id, actor)
	verify: (raw: string) => svc(c).verify(raw)
	grantable: (user: any) => svc(c).grantable(user)
}

// Every feature registers its API next to its admin screens:
//   registerEndpoint(app, { method, path, capability, tag, summary, handler })
// See api/server/router.js for the shape, CLAUDE.md for the rule.
export registerEndpoint := (app: Container, ep: any) =>
	app.single `api.ep:${ep.method} ${ep.path}`, => ep

rootOf := (c: Container) =>
	node .= c as any
	while node.parent
		node = node.parent
	node

export endpoints := (c: Container) =>
	root := rootOf c
	out := [] as any[]
	for [token] of root.bindings
		out.push root.resolve(token) if token.startsWith('api.ep:')
	ORDER := ['Account', 'Content', 'Media', 'Taxonomies', 'Forms', 'Localisation', 'Users', 'Roles', 'Settings', 'SEO', 'Consent', 'Audit', 'Mail', 'Maintenance']
	rank := (t: string) =>
		i := ORDER.indexOf(t)
		i === -1 ? 99 : i
	out.sort (a, b) => (rank(a.tag) - rank(b.tag)) || a.path.localeCompare(b.path) || a.method.localeCompare(b.method)

export openapiDoc := async (c: Container) =>
	site := await settings(c).get 'site'
	openapi endpoints(c), { origin: siteOrigin(), title: site.title }

CORS := {
	'access-control-allow-origin': '*'
	'access-control-allow-methods': 'GET, POST, PATCH, PUT, DELETE, OPTIONS'
	'access-control-allow-headers': 'authorization, content-type'
	'access-control-max-age': '86400'
}

respond := (status: number, body: any) =>
	new Response(JSON.stringify(body), { status, headers: { 'content-type': 'application/json; charset=utf-8', 'cache-control': 'no-store', ...CORS } })

errorBody := (status: number, message: string, extra: any = {}) => respond status, { error: { status, message, ...extra } }

// Valibot issues → [{ field, message }] for API clients.
issuesOf := (issues: any[]) => (issues ?? []).map((i) => ({ field: i.path?.map((p: any) => p.key).join('.') || undefined, message: i.message }))
export validationError := (issues: any[]) => apiError 400, 'Invalid input', { issues: issuesOf(issues) }

// The whole /api/v1 request: CORS preflight, key, capability, rate limit,
// body, dispatch, error mapping. Bearer keys only — cookies are ignored, so
// a browser session can never be used cross-site against this API.
export handleApi := async (event: any, path: string) =>
	method := event.request.method
	return new Response(null, { status: 204, headers: CORS }) if method === 'OPTIONS'
	c := event.locals.container
	try
		if path === '/openapi.json' && method === 'GET'
			return respond 200, await openapiDoc(c)
		found := match endpoints(c), method, path
		return errorBody(404, `No endpoint ${method} ${path}`) unless found
		return errorBody(405, `${method} is not supported on ${path}`) if found.notAllowed
		header := event.request.headers.get('authorization') ?? ''
		token := header.replace(/^Bearer\s+/i, '')
		return errorBody(401, 'Send an API key: Authorization: Bearer ncms_…') unless token && token !== header
		user := await apiKeys(c).verify token
		return errorBody(401, 'The API key is invalid, expired or revoked') unless user
		unless await auth(c).rateLimit('api:key', String(user.apiKey.id), 600, 60000)
			return errorBody 429, 'Rate limit: 600 requests per minute per key'
		{ ep, params } := found
		if ep.capability && !can(user, ep.capability)
			return errorBody 403, `This key lacks the "${ep.capability}" capability`
		event.locals.user = user
		body .= null as any
		if !ep.raw && method !== 'GET' && method !== 'DELETE'
			text := await event.request.text()
			if text.trim()
				try
					body = JSON.parse(text)
				catch
					return errorBody 400, 'The body must be JSON'
			body ??= {}
		query := Object.fromEntries(event.url.searchParams)
		result := await ep.handler { c, user, params, query, body, event }
		return result if result instanceof Response
		status := method === 'POST' && ep.created ? 201 : 200
		wrapped := result && typeof result === 'object' && 'data' in result && Object.keys(result).every((k) => k === 'data' || k === 'meta')
		respond(status, wrapped ? result : { data: result ?? null })
	catch e
		return errorBody(e.status, e.message, e.extra) if e instanceof ApiError
		// SvelteKit error() from shared helpers (requireCap, typeOr…).
		return errorBody(e.status, e.body?.message ?? 'Error') if typeof e?.status === 'number' && e?.body
		console.error 'api:', method, path, e
		errorBody 500, 'Internal error'

// Shared helpers for endpoint handlers.
export paging := (query: any, max = 100) =>
	limit := Math.min(max, Math.max(1, Number(query.limit) || 50))
	offset := Math.max(0, Number(query.offset) || 0)
	{ limit, offset }

export need := (value: any, message: string, status = 404) =>
	throw apiError(status, message) unless value
	value

export allow := (ok: any, message = 'This key may not do that') =>
	throw apiError(403, message) unless ok
	return
