import type { Container } from '@human-synthesis/norns/server'
import type { ProtectionRepo } from './repo'
import { siteverify } from './turnstile.js'
import { HONEYPOT, TOKEN, TURNSTILE_FIELD, actionFor, decide } from '../shared/guard.js'
import { envVar, sealValue, openValue, signValue } from '../../settings/server/public'
import { audit, clientIp } from '../../auth/server/public'
import { tFor } from '../../i18n/server/public'

DAY := 24 * 60 * 60 * 1000
TOKEN_MAX_AGE := 3 * DAY
TTL := 15000
SECRET_PURPOSE := 'turnstile-secret'
// Per-isolate cache of config + overrides (read on every page render).
cache := new WeakMap<any, { at: number, config: any, overrides: Record<string, any> }>()

export type FormDef =
	id: string
	label: string
	group?: string
	path?: string
	audience?: string     // 'staff' (English messages) | 'public'
	minMs?: number        // fastest plausible human submission
	turnstile?: boolean   // default for the Turnstile switch (true)
	honeypot?: boolean    // default for the honeypot switch (true)

dayString := (ms: number) => new Date(ms).toISOString().slice(0, 10)

export class ProtectionService
	repo: ProtectionRepo
	c: Container
	constructor(@repo: ProtectionRepo, @c: Container)

	root(): any
		node .= @c as any
		while node.parent
			node = node.parent
		node

	// Forms declared in code (registerForm) and the ones features build at
	// runtime (registerFormSource, e.g. every form made in Forms).
	async definitions(): Promise<FormDef[]>
		out := [] as FormDef[]
		node := @root()
		for [token] of node.bindings
			out.push node.resolve(token) if token.startsWith('protection.form:')
		for [token] of node.bindings
			if token.startsWith('protection.source:')
				try
					out.push(...(await node.resolve(token)(@c)))
				catch e
					console.error 'protection: form source failed', token, e
		out

	async definition(id: string): Promise<FormDef>
		node := @root()
		token := `protection.form:${id}`
		return node.resolve(token) if node.bindings.has(token)
		found := (await @definitions()).find (d) => d.id === id
		found ?? { id, label: id, turnstile: true }

	async state()
		db := (@repo as any).db
		key := db.raw ?? db
		hit := cache.get key
		return hit if hit && Date.now() - hit.at < TTL
		config := await @repo.config()
		overrides := {} as Record<string, any>
		for row of await @repo.overrides()
			overrides[row.form_id] = row
		fresh := { at: Date.now(), config, overrides }
		cache.set key, fresh
		fresh

	forget()
		db := (@repo as any).db
		cache.delete db.raw ?? db

	// Keys and switches as they apply now. Env vars win over the admin:
	// TURNSTILE_SITE_KEY / TURNSTILE_SECRET_KEY, and TURNSTILE_ENABLED=false
	// switches Turnstile off everywhere (a way back in if the keys are wrong).
	async keys()
		{ config } := await @state()
		envSite := envVar 'TURNSTILE_SITE_KEY'
		envSecret := envVar 'TURNSTILE_SECRET_KEY'
		killed := envVar('TURNSTILE_ENABLED') === 'false'
		siteKey := envSite || config?.site_key || ''
		secret := envSecret || openValue(config?.secret, SECRET_PURPOSE) || ''
		{
			enabled: !!config?.enabled
			siteKey
			secret
			appearance: config?.appearance || 'always'
			verifiedAt: config?.verified_at ?? null
			fromEnv: { siteKey: !!envSite, secret: !!envSecret }
			killed
			active: !killed && !!config?.enabled && !!siteKey && !!secret
		}

	// For the admin and the API: never the secret itself.
	async config()
		k := await @keys()
		{ config } := await @state()
		{
			enabled: k.enabled
			active: k.active
			siteKey: k.siteKey
			secretSet: !!k.secret
			secretHint: k.secret ? `…${k.secret.slice(-4)}` : ''
			appearance: k.appearance
			verifiedAt: k.verifiedAt
			fromEnv: k.fromEnv
			killed: k.killed
			updatedAt: config?.updated_at ?? null
		}

	async saveConfig(input: Record<string, any>, actor: any, meta: Record<string, any> = {})
		{ config } := await @state()
		k := await @keys()
		siteKey := input.siteKey === undefined ? (config?.site_key ?? '') : String(input.siteKey).trim()
		newSecret := String(input.secretKey ?? '').trim()
		sealed .= config?.secret ?? null
		sealed = null if input.clearSecret
		sealed = sealValue(newSecret, SECRET_PURPOSE) if newSecret
		appearance := input.appearance === undefined ? (config?.appearance ?? 'always') : (input.appearance === 'interaction-only' ? 'interaction-only' : 'always')
		enabled := input.enabled === undefined ? !!config?.enabled : !!input.enabled
		effectiveSite := k.fromEnv.siteKey ? k.siteKey : siteKey
		effectiveSecret := k.fromEnv.secret ? k.secret : (newSecret || (sealed ? openValue(sealed, SECRET_PURPOSE) : ''))
		if enabled && !(effectiveSite && effectiveSecret)
			return { ok: false, error: 'Add the site key and the secret key before switching Turnstile on', field: (effectiveSite ? 'secretKey' : 'siteKey') }
		verifiedAt .= config?.verified_at ?? null
		warning .= ''
		if newSecret && !k.fromEnv.secret
			// A dummy token: a good secret answers invalid-input-response, a
			// bad one invalid-input-secret.
			r := await siteverify newSecret, 'norns-cms-key-check', null
			if r.errors.includes 'invalid-input-secret'
				return { ok: false, error: 'Cloudflare does not recognise this secret key', field: 'secretKey' }
			if r.unavailable
				verifiedAt = null
				warning = 'Saved, but Cloudflare could not be reached to check the secret key'
			else
				verifiedAt = Date.now()
		await @repo.saveConfig { enabled, site_key: siteKey, secret: sealed, appearance, verified_at: verifiedAt }, actor?.id ?? null
		@forget()
		await audit(@c).log actor, 'protection.config', 'turnstile', { ...meta, enabled, appearance, siteKey: !!siteKey, secretChanged: !!newSecret || !!input.clearSecret }
		{ ok: true, warning }

	// Every registered form with its switches and 30-day counts.
	async forms()
		defs := await @definitions()
		{ overrides } := await @state()
		k := await @keys()
		counts := {} as Record<string, Record<string, number>>
		for row of await @repo.stats(dayString(Date.now() - 29 * DAY))
			(counts[row.form_id] ??= {})[row.outcome] = Number(row.n)
		defs.map (d) =>
			o := overrides[d.id]
			{
				...d
				turnstileMode: o?.turnstile ?? 'default'
				honeypotOn: decide(d, o, true).honeypot
				applies: decide(d, o, k.active)
				counts: counts[d.id] ?? {}
			}

	async saveForm(id: string, input: Record<string, any>, actor: any, meta: Record<string, any> = {})
		defs := await @definitions()
		def := defs.find (d) => d.id === id
		return { ok: false, error: 'Form not found' } unless def
		{ overrides } := await @state()
		current := decide def, overrides[id], true
		mode := input.turnstile ?? overrides[id]?.turnstile ?? 'default'
		return { ok: false, error: 'turnstile is default, on or off', field: 'turnstile' } unless ['default', 'on', 'off'].includes(mode)
		honeypot := input.honeypot === undefined ? current.honeypot : !!input.honeypot
		await @repo.saveForm id, mode, honeypot
		@forget()
		await audit(@c).log actor, 'protection.form', id, { ...meta, turnstile: mode, honeypot }
		{ ok: true }

	// What FormGuard needs on every page: the switches per form, the site
	// key when Turnstile is on, and a signed render time.
	async clientConfig()
		k := await @keys()
		{ overrides } := await @state()
		node := @root()
		forms := {} as Record<string, any>
		for [token] of node.bindings
			if token.startsWith('protection.form:')
				d := node.resolve token
				forms[d.id] = decide d, overrides[d.id], k.active
		for id of Object.keys(overrides)
			forms[id] ??= decide { id, turnstile: true }, overrides[id], k.active
		ts := String(Date.now())
		{
			active: k.active
			siteKey: k.active ? k.siteKey : ''
			appearance: k.appearance
			token: `${ts}.${signValue(ts, 'form-token').slice(0, 32)}`
			forms
		}

	// The check every protected submission goes through. Returns
	// { ok } or { ok: false, reason } and counts the outcome.
	async check(event: any, formId: string, fd: FormData)
		def := await @definition formId
		{ overrides } := await @state()
		k := await @keys()
		applies := decide def, overrides[formId], k.active
		if applies.honeypot
			return await @verdict(formId, 'honeypot') if String(fd.get(HONEYPOT) ?? '').trim() !== ''
			[ts, sig] := String(fd.get(TOKEN) ?? '').split('.')
			age := Date.now() - Number(ts)
			return await @verdict(formId, 'token') unless ts && sig && sig === signValue(ts, 'form-token').slice(0, 32) && age < TOKEN_MAX_AGE && age > -60000
			return await @verdict(formId, 'too_fast') if age < (def.minMs ?? 0)
		if applies.turnstile
			r := await siteverify k.secret, fd.get(TURNSTILE_FIELD), clientIp(event)
			if r.unavailable
				// Cloudflare down: let people through rather than lock everyone
				// out of sign-in; the honeypot still applied. Counted apart.
				await @repo.count formId, 'unverified'
				return { ok: true, unverified: true }
			return await @verdict(formId, 'turnstile') unless r.success && (!r.action || r.action === actionFor(formId))
		await @repo.count formId, 'passed'
		{ ok: true }

	async verdict(formId: string, reason: string)
		await @repo.count formId, reason
		{ ok: false, reason, silent: reason === 'honeypot' || reason === 'too_fast' }

	async message(reason: string, event: any, formId: string)
		def := await @definition formId
		lang := def.audience === 'staff' ? 'en' : (event.locals.locale?.language ?? 'en')
		t := await tFor @c, lang
		t (reason === 'token' ? 'guard.expired' : (reason === 'turnstile' ? 'guard.turnstile' : 'guard.failed'))

	async purge()
		{ stats: await @repo.purge(dayString(Date.now() - 180 * DAY)) }
