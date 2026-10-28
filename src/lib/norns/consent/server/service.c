import { randomBytes, createHash } from 'node:crypto'
import { getScope } from '@human-synthesis/norns/server'
import type { Container } from '@human-synthesis/norns/server'
import type { ConsentRepo } from './repo'
import { PROVIDERS, validateConfig, parseCookieLines, GOOGLE_CONSENT_DEFAULT } from './providers.js'
import { LOADER } from './loader.js'
import { CATEGORIES, encodeConsent, decodeConsent, defaultsFor, regionOf, modeFor } from '../shared/config'
import type { Choice } from '../shared/config'
import { settings } from '../../settings/server/public'
import { tFor } from '../../i18n/server/public'

ACTIONS := ['accept_all', 'reject_all', 'custom', 'withdraw', 'acknowledge']
MONTH := 30 * 24 * 60 * 60 * 1000

// Enabled scripts per database (D1 binding / SQLite handle), cached briefly
// per isolate; every public page needs them.
scriptCache := new WeakMap<any, { at: number, rows: any[] }>()

export NECESSARY_COOKIES := [
	{ name: 'cms_consent', duration: 'per settings (max 13 months)', purpose: 'Remembers your cookie choice' }
	{ name: 'cms_session', duration: 'session length', purpose: 'Keeps you signed in (only when you sign in)' }
	{ name: 'cms_mfa', duration: '5 minutes', purpose: 'Two-factor sign-in step' }
]

export class ConsentService
	repo: ConsentRepo
	db: any
	c: Container
	constructor(@repo: ConsentRepo, @db: any, @c: Container)

	async config()
		await settings(@c).localized 'consent'

	// --- per request ---------------------------------------------------------

	// What applies to this visitor right now.
	async state(event: any)
		cfg := await @config()
		country := String(event.platform?.cf?.country ?? event.request.headers.get('cf-ipcountry') ?? '').toUpperCase()
		region := regionOf country
		mode := modeFor region, cfg
		gpc := event.request.headers.get('sec-gpc') === '1'
		version := Number(cfg.policyVersion) || 1
		stored := decodeConsent event.cookies.get('cms_consent')
		given := !!stored && stored.version === version
		choice := if given then stored!.choice else defaultsFor(mode, gpc)
		enabled := cfg.enabled !== false
		{
			enabled
			given
			consentId: stored?.id ?? null
			version
			mode
			region
			country
			gpc
			choice: enabled ? choice : { preferences: true, analytics: true, marketing: true }
			showBanner: enabled && !given
		}

	async enabledScripts()
		key := @db.raw ?? @db
		hit := scriptCache.get key
		return hit.rows if hit && Date.now() - hit.at < 15000
		rows .= [] as any[]
		try
			rows = await @repo.scripts(true)
		catch
			rows = []
		scriptCache.set key, { at: Date.now(), rows }
		rows

	render(script: any)
		p := (PROVIDERS as any)[script.provider]
		return { head: '', body: '' } unless p
		out := p.render(script.config ?? {}, script.code)
		{ head: out.head ?? '', body: out.body ?? '' }

	// HTML to inject: consent-mode defaults, scripts (live when allowed,
	// otherwise inert in a <template>), the loader with the current choice.
	async injection(state: any, nonce: string)
		scripts := await @enabledScripts()
		head := [] as string[]
		body := [] as string[]
		attr := nonce ? ` nonce="${nonce}"` : ''
		if scripts.some((s) => (PROVIDERS as any)[s.provider]?.google)
			head.push GOOGLE_CONSENT_DEFAULT(state.choice).replace('<script>', `<script${attr}>`)
		for s of scripts
			{ head: h, body: b } := @render s
			allowed := !!(state.choice as any)[s.category]
			placeHead := s.placement !== 'body'
			for [html, inHead] of [[h, placeHead], [b, false]] as [string, boolean][]
				continue unless html
				wrapped := allowed ? html : `<template data-consent="${s.category}">${html}</template>`
				target := inHead ? head : body
				target.push wrapped
		consent := JSON.stringify(state.choice).replace(/</g, '\\u003c')
		body.push `<script${attr}>window.__cmsConsent=${consent};${LOADER}</script>`
		{ head: head.join(''), body: body.join('') }

	// --- recording a choice ----------------------------------------------------

	async record(event: any, input: Record<string, any>, user: any)
		action := ACTIONS.includes(input.action) ? input.action : 'custom'
		state := await @state event
		cfg := await @config()
		yes := (v: any) => v === true || v === 'on' || v === 'true' || v === '1'
		choice := if action === 'accept_all' || action === 'acknowledge'
			{ preferences: true, analytics: true, marketing: !state.gpc || yes(input.marketing) }
		else if action === 'reject_all' || action === 'withdraw'
			{ preferences: false, analytics: false, marketing: false }
		else
			{ preferences: yes(input.preferences), analytics: yes(input.analytics), marketing: yes(input.marketing) }
		consentId := state.consentId ?? randomBytes(16).toString('hex')
		bannerHash := createHash('sha256').update(`${cfg.bannerTitle}\n${cfg.bannerText}\n${state.version}\n${event.locals?.locale?.language ?? ''}`).digest('hex').slice(0, 16)
		locale := event.locals?.locale?.language ?? ''
		await @repo.record { consentId, userId: user?.kind === 'client' ? user.id : null, version: state.version, action, choice, country: state.country, mode: state.mode, gpc: state.gpc, bannerHash, locale }
		days := Math.min(395, Math.max(1, Number(cfg.cookieDays) || 180))
		{ consentId, choice, cookie: encodeConsent(consentId, state.version, choice as Choice), maxAge: days * 24 * 60 * 60 }

	// --- declaration -----------------------------------------------------------

	async declaration()
		cfg := await @config()
		scripts := await @enabledScripts()
		t := await tFor @c, (getScope()?.event?.locals?.locale?.language ?? 'en')
		groups := [
			{ id: 'necessary', label: t('consent.necessary'), text: cfg.necessaryText, required: true, cookies: [...NECESSARY_COOKIES], services: [] as string[] }
			{ id: 'preferences', label: cfg.preferencesLabel, text: cfg.preferencesText, required: false, cookies: [] as any[], services: [] as string[] }
			{ id: 'analytics', label: cfg.analyticsLabel, text: cfg.analyticsText, required: false, cookies: [] as any[], services: [] as string[] }
			{ id: 'marketing', label: cfg.marketingLabel, text: cfg.marketingText, required: false, cookies: [] as any[], services: [] as string[] }
		]
		for s of scripts
			g := groups.find((x) => x.id === s.category)
			continue unless g
			p := (PROVIDERS as any)[s.provider]
			g.services.push s.name || p?.label || s.provider
			cookies := s.provider === 'custom' ? parseCookieLines(s.cookies) : [...(p?.cookies ?? []), ...parseCookieLines(s.cookies)]
			for ck of cookies
				g.cookies.push { ...ck, service: s.name || p?.label }
		marketing := groups.find((x) => x.id === 'marketing')
		marketing?.services.push t('consent.embedded')
		patterns := {} as Record<string, string[]>
		for cat of CATEGORIES
			patterns[cat] = scripts.filter((s) => s.category === cat).flatMap((s) => (PROVIDERS as any)[s.provider]?.patterns ?? [])
		{ groups, patterns, policyUrl: cfg.policyUrl, policyLabel: cfg.policyLabel }

	// Banner texts + declaration, for the site layout.
	async banner(state: any)
		cfg := await @config()
		decl := await @declaration()
		{
			title: cfg.bannerTitle
			text: cfg.bannerText
			policyUrl: cfg.policyUrl
			policyLabel: cfg.policyLabel
			mode: state.mode
			gpc: state.gpc
			groups: decl.groups
			patterns: decl.patterns
		}

	// --- admin -----------------------------------------------------------------

	providers()
		Object.entries(PROVIDERS).map ([id, p]: [string, any]) => ({ id, label: p.label, category: p.category, fields: p.fields, cookies: p.cookies })

	scripts()
		@repo.scripts()

	script(id: number)
		@repo.script id

	async saveScript(id: number | null, raw: Record<string, any>, user: any)
		provider := String(raw.provider ?? '')
		p := (PROVIDERS as any)[provider]
		return { ok: false, error: 'Pick a provider', field: 'provider' } unless p
		name := String(raw.name ?? '').trim() || p.label
		category := CATEGORIES.includes(raw.category) ? raw.category : p.category
		cfgRaw := {} as Record<string, any>
		for [k, value] of Object.entries(raw)
			cfgRaw[k.slice(7)] = value if k.startsWith('config_')
		checked := validateConfig provider, cfgRaw
		return checked unless checked.ok
		code := provider === 'custom' ? String(raw.code ?? '') : ''
		return { ok: false, error: 'Paste the code to inject', field: 'code' } if provider === 'custom' && !code.trim()
		values := {
			name
			provider
			category
			placement: raw.placement === 'body' ? 'body' : 'head'
			config: checked.config
			code
			cookies: String(raw.cookies ?? '')
			enabled: raw.enabled === 'on' || raw.enabled === true || raw.enabled === 'true'
			sort: Number(raw.sort) || 0
		}
		savedId := await @repo.saveScript id, values, user?.id ?? null
		scriptCache.delete @db.raw ?? @db
		{ ok: true, id: savedId, name, provider }

	async removeScript(id: number)
		await @repo.removeScript id
		scriptCache.delete @db.raw ?? @db
		return

	async bumpVersion(user: any)
		cfg := await @config()
		next := (Number(cfg.policyVersion) || 1) + 1
		await settings(@c).patch 'consent', { policyVersion: next }, user?.id ?? null
		next

	records(q: any = {})
		@repo.records q

	async stats(days = 30)
		raw := await @repo.stats Date.now() - days * 24 * 60 * 60 * 1000
		count := (a: string) => Number(raw.byAction.find((r: any) => r.action === a)?.n ?? 0)
		total := raw.byAction.reduce(((sum: number, r: any) => sum + Number(r.n)), 0)
		choices := Number(raw.cats?.total ?? 0)
		pct := (n: any) => choices ? Math.round((Number(n ?? 0) / choices) * 100) : 0
		{
			days
			total
			acceptAll: count('accept_all')
			rejectAll: count('reject_all')
			custom: count('custom')
			withdraw: count('withdraw')
			analyticsRate: pct(raw.cats?.analytics)
			marketingRate: pct(raw.cats?.marketing)
			preferencesRate: pct(raw.cats?.preferences)
			byCountry: raw.byCountry
		}

	async purge()
		cfg := await @config()
		months := Math.max(1, Number(cfg.retentionMonths) || 24)
		await @repo.purge Date.now() - months * MONTH
