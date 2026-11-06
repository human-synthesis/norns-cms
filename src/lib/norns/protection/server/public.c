import type { Container } from '@human-synthesis/norns/server'
import { fail } from '@sveltejs/kit'
import { siteOrigin as siteOriginOf, backgroundTask } from '../../settings/server/public'
import type { ProtectionService, FormDef } from './service'
import type { BlockedPaths } from './paths'
import type { CountryFilter } from './countries'
import type { CloudflareFirewall } from './cloudflare'

export type { FormDef }
export { HONEYPOT, TOKEN, TURNSTILE_FIELD } from '../shared/guard.js'
export { looksLikeProbe, suggestPattern, matches as pathMatches, normalizePattern } from '../shared/paths.js'

svc := (c: Container) => c.resolve('protection.service') as ProtectionService
paths := (c: Container) => c.resolve('protection.paths') as BlockedPaths
countries := (c: Container) => c.resolve('protection.countries') as CountryFilter
firewall := (c: Container) => c.resolve('protection.cloudflare') as CloudflareFirewall

// Form protection: every public form is registered here, gets a managed
// honeypot and signed render time, and Cloudflare Turnstile when keys are
// set (Admin → Form protection). Outcomes are counted per form and day.
export protection := (c: Container) => {
	config: () => svc(c).config()
	saveConfig: (input: any, actor: any, meta?: any) => svc(c).saveConfig(input, actor, meta)
	forms: () => svc(c).forms()
	saveForm: (id: string, input: any, actor: any, meta?: any) => svc(c).saveForm(id, input, actor, meta)
	definitions: () => svc(c).definitions()
	clientConfig: () => svc(c).clientConfig()
	check: (event: any, formId: string, fd: FormData) => svc(c).check(event, formId, fd)
	message: (reason: string, event: any, formId: string) => svc(c).message(reason, event, formId)
	purge: () => svc(c).purge()
}

// A form a feature owns, declared from its module.c:
// registerForm(app, { id: 'auth.member-login', label: 'Member sign-in', group: 'Accounts', path: '/account/login', minMs: 0 }).
export registerForm := (app: Container, def: FormDef) =>
	app.single `protection.form:${def.id}`, => def

// Forms made at runtime (one per row, like Forms): fn(c) → FormDef[].
export registerFormSource := (app: Container, id: string, fn: (c: Container) => Promise<FormDef[]>) =>
	app.single `protection.source:${id}`, => fn

// Wraps SvelteKit actions (page.actions output): each named action checks
// its form first. Reads a clone of the body so the action reads it as usual.
// Failure: fail(400, { guard, guardForm, values }) — FormGuard shows it.
export protect := (actions: Record<string, any>, ids: Record<string, string>) =>
	out := { ...actions }
	for name of Object.keys(ids)
		inner := actions[name]
		formId := ids[name]
		throw new Error(`protect(): no action "${name}"`) unless inner
		out[name] = async (event: any) =>
			c := event.locals.container
			fd := await event.request.clone().formData()
			v := await svc(c).check event, formId, fd
			return inner(event) if v.ok
			values := {} as Record<string, string>
			for [k, val] of fd.entries()
				values[k] = val if typeof val === 'string' && !k.startsWith('_') && !/password|confirm|code|turnstile/i.test(k)
			fail 400, { guard: await svc(c).message(v.reason, event, formId), guardForm: formId, values }
	out

// Blocked paths (scanner probes): the list, checks and the 404-log
// suggestions.
export blocked := (c: Container) => {
	list: () => paths(c).list()
	add: (input: { pattern: string, note?: string, source?: string }, user?: any) => paths(c).add(input, user)
	update: (id: number, fields: { enabled?: boolean, note?: string }) => paths(c).update(id, fields)
	remove: (id: number) => paths(c).remove(id)
	restoreDefaults: () => paths(c).restoreDefaults()
	check: (path: string) => paths(c).check(path)
	match: (path: string) => paths(c).match(path)
	suggestions: (notFound: any[]) => paths(c).suggestions(notFound)
	flushHits: () => paths(c).flushHits()
}

// The country filter: allowed / blocked countries, who is where.
export countryFilter := (c: Container) => {
	filter: () => countries(c).filter()
	save: (input: any, ctx?: { user?: any, country?: string }) => countries(c).save(input, ctx)
	status: (event?: any) => countries(c).status(event)
	stats: () => countries(c).stats()
	clearStats: () => countries(c).clearStats()
	flushHits: () => countries(c).flushHits()
}

export { countryOf } from './countries'

// Cloudflare firewall: the same rules as WAF custom rules on the zone.
export cloudflare := (c: Container) => {
	status: () => firewall(c).status()
	save: (input: Record<string, any>, user?: any) => firewall(c).save(input, user)
	sync: () => firewall(c).sync()
	remove: () => firewall(c).sync({ remove: true })
	preview: async () =>
		s := await firewall(c).status()
		firewall(c).desired(s.zoneName || new URL(siteOriginOf()).hostname)
}

EMPTY_403 := => new Response(null, { status: 403, headers: { 'cache-control': 'no-store', connection: 'close', 'x-robots-tag': 'noindex' } })

// First in the handle chain: a blocked path, or a visitor from a country
// the site is not open to, gets an empty 403 before sessions, locales,
// rendering or the 404 log. Any method.
export blockHandle := =>
	async ({ event, resolve }: any) =>
		c := event.locals.container
		return resolve(event) unless c
		wait := backgroundTask()
		rule := await paths(c).match event.url.pathname
		if rule
			paths(c).hit rule.id, wait
			return EMPTY_403()
		refused := await countries(c).refuse event
		if refused
			countries(c).hit refused, wait
			return EMPTY_403()
		resolve event
