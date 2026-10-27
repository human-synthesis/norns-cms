import type { Container } from '@human-synthesis/norns/server'
import { getScope } from '@human-synthesis/norns/server'
import type { I18nRepo } from './repo'
import { basePath, baseFor, offers, resolveLocale, hreflangs, segmentOf } from '../shared/urls.js'
import { KNOWN_LANGUAGES, dirOf, LANGUAGE_RE, defaultSegment } from '../shared/languages.js'
import { builtIn } from '../shared/t.js'
import { KEYS, CATALOGS } from '../shared/catalog.js'
import { envVar } from '../../settings/server/public'

TTL := 15000
cache := new WeakMap<any, { at: number, cfg: any }>()
stringCache := new WeakMap<any, Map<string, { at: number, map: Record<string, string> }>>()

// What the site looks like with no configuration (tests, a fresh database
// before migrations): today's URLs, English, one localisation.
export DEFAULT_CONFIG := {
	primaryHost: 'localhost'
	protocol: 'https:'
	languages: { en: { code: 'en', name: 'English', native: 'English', dir: 'ltr', fallback: '', paths: {}, enabled: true } }
	markets: [{ id: 0, code: 'default', name: 'Default', mode: 'path', prefix: '', host: '', languages: [{ code: 'en', segment: 'en' }], defaultLanguage: 'en', prefixDefault: false, countries: [], contentFallback: false, isPrimary: true, enabled: true, sort: 0 }]
}

WORDS := { authors: 'authors', category: 'category', tag: 'tag' } as Record<string, string>

slug := (s: string) => String(s ?? '').trim().toLowerCase().replace(/[^a-z0-9-]+/g, '-').replace(/^-+|-+$/g, '')

export class I18nService
	repo: I18nRepo
	db: any
	c: Container
	constructor(@repo: I18nRepo, @db: any, @c: Container)

	primaryHost()
		url := envVar 'CMS_PUBLIC_URL'
		try
			return new URL(url).host if url
		catch
			url
		getScope()?.event?.url?.host ?? 'localhost'

	protocol()
		url := envVar 'CMS_PUBLIC_URL'
		return 'https:' if url?.startsWith('https:')
		getScope()?.event?.url?.protocol ?? 'https:'

	// The configuration, cached per isolate and database for a few seconds.
	async config()
		key := @db.raw ?? @db
		hit := cache.get key
		if hit && Date.now() - hit.at < TTL
			return hit.cfg
		cfg .= null as any
		try
			languages := {} as Record<string, any>
			for l of await @repo.languages()
				languages[l.code] = l
			markets := (await @repo.markets()).filter((m) => m.languages.length)
			cfg = markets.length ? { primaryHost: @primaryHost(), protocol: @protocol(), languages, markets } : { ...DEFAULT_CONFIG, primaryHost: @primaryHost() }
		catch
			cfg = { ...DEFAULT_CONFIG, primaryHost: @primaryHost() }
		cache.set key, { at: Date.now(), cfg }
		cfg

	invalidate()
		cache.delete @db.raw ?? @db
		stringCache.delete @db.raw ?? @db

	// Synchronous view for code that cannot await (URL building inside
	// services): the configuration loaded for the current request, else
	// the last one loaded for this database, else the default.
	sync()
		getScope()?.event?.locals?.i18nConfig ?? cache.get(@db.raw ?? @db)?.cfg ?? DEFAULT_CONFIG

	// Enabled localisations; never empty (the default keeps the site up).
	markets()
		ms := @sync().markets.filter((m: any) => m.enabled)
		ms.length ? ms : DEFAULT_CONFIG.markets

	primary()
		ms := @markets()
		ms.find((m: any) => m.isPrimary) ?? ms[0]

	language(code: string)
		@sync().languages[code] ?? { code, name: code, native: code, dir: dirOf(code), fallback: '', paths: {} }

	// Every language some enabled localisation offers, primary's default first.
	contentLanguages(): string[]
		out := [] as string[]
		p := @primary()
		out.push p.defaultLanguage if p
		for m of @markets()
			for l of m.languages
				out.push l.code unless out.includes l.code
		out

	defaultLanguage()
		@primary()?.defaultLanguage ?? 'en'

	// code → [code, its fallback, …, base language], no repeats.
	chain(code: string): string[]
		out := [] as string[]
		cur .= code
		while cur && !out.includes(cur) && out.length < 5
			out.push cur
			cur = @language(cur).fallback || (cur.includes('-') ? cur.split('-')[0] : '')
		out

	// Where a language is shown: the current request's localisation if it
	// offers it, else the primary, else the first that does.
	marketFor(code: string)
		current := getScope()?.event?.locals?.locale?.market
		ms := @markets()
		if current
			m := ms.find((x: any) => x.code === current.code)
			return m if m && offers(m, code)
		p := @primary()
		return p if p && offers(p, code)
		ms.find((m: any) => offers(m, code)) ?? p

	// { origin, base, market } for a language (in a given localisation).
	link(code: string, marketCode?: string)
		cfg := @sync()
		m := marketCode ? (@markets().find((x: any) => x.code === marketCode) ?? @marketFor(code)) : @marketFor(code)
		{ origin, path } := baseFor cfg, m, code
		{ origin, base: path, market: m }

	// Every place a language is offered (for hreflang and redirects).
	placesOf(code: string)
		cfg := @sync()
		@markets().filter((m: any) => offers(m, code)).map (m: any) =>
			{ origin, path } := baseFor cfg, m, code
			{ market: m, origin, base: path, hreflang: hreflangs(m, code) }

	// URL words for a language: type sections and archive words.
	typeSegment(code: string, typeId: string, fallback: string)
		paths := @language(code).paths ?? {}
		String(paths[typeId] || fallback || '').replace(/^\/+|\/+$/g, '')

	word(code: string, key: string)
		paths := @language(code).paths ?? {}
		paths[key] || WORDS[key] || key

	// --- requests ------------------------------------------------------------

	resolve(host: string, path: string, accept: string)
		resolveLocale @sync(), host, path, accept

	// The locale object a request carries in event.locals.locale.
	describe(result: any)
		cfg := @sync()
		{ market, language } := result
		info := @language language
		{ origin } := baseFor cfg, market, language
		{
			market: { code: market.code, name: market.name, mode: market.mode, countries: market.countries, contentFallback: market.contentFallback, isPrimary: market.isPrimary, defaultLanguage: market.defaultLanguage, languages: market.languages.map((l: any) => l.code) }
			language
			name: info.native || info.name
			dir: info.dir || dirOf(language)
			base: result.base
			origin
			rest: result.rest ?? '/'
			chain: @chain(language)
		}

	// Locale for pages that are not localised by URL (admin, account, API):
	// the language from the cms_lang cookie when this host offers it.
	systemLocale(host: string, cookieLang: string | undefined)
		cfg := @sync()
		h := host.toLowerCase().split(':')[0]
		ms := @markets()
		market := ms.find((m: any) => m.mode !== 'path' && m.host?.toLowerCase() === h) ?? @primary()
		language := cookieLang && offers(market, cookieLang) ? cookieLang : market.defaultLanguage
		@describe { market, language, base: basePath(market, language), rest: '/' }

	// --- interface texts ------------------------------------------------------

	async strings(code: string): Promise<Record<string, string>>
		key := @db.raw ?? @db
		byDb .= stringCache.get key
		unless byDb
			byDb = new Map()
			stringCache.set key, byDb
		hit := byDb.get code
		return hit.map if hit && Date.now() - hit.at < TTL
		chain := @chain code
		map := builtIn chain
		rows .= [] as any[]
		try
			rows = await @repo.strings(chain)
		catch
			rows = []
		for c of [...chain].reverse()
			for r of rows
				map[r.key] = r.value if r.locale === c
		byDb.set code, { at: Date.now(), map }
		map

	async stringTable(code: string)
		own := (await @repo.strings [code])
		overrides := Object.fromEntries own.map((r: any) => [r.key, r.value])
		chain := @chain code
		inherited := builtIn chain.slice(1).length ? chain.slice(1) : ['en']
		catalogue := CATALOGS[code] ?? CATALOGS[code.split('-')[0]] ?? null
		KEYS.map (k: string) => ({
			key: k
			english: CATALOGS.en[k]
			builtIn: catalogue?.[k] ?? null
			inherited: inherited[k]
			override: overrides[k] ?? null
			value: overrides[k] ?? catalogue?.[k] ?? inherited[k]
		})

	async setStrings(code: string, values: Record<string, string>)
		known := new Set(KEYS)
		n .= 0
		for [k, v] of Object.entries(values)
			continue unless known.has k
			await @repo.setString code, k, String(v ?? '').slice(0, 2000)
			n++
		@invalidate()
		n

	// --- administration --------------------------------------------------------

	known()
		KNOWN_LANGUAGES

	async allLanguages()
		langs := await @repo.languages()
		out := [] as any[]
		for l of langs
			out.push { ...l, entries: await @repo.entriesIn(l.code), builtIn: !!(CATALOGS[l.code] ?? CATALOGS[l.code.split('-')[0]]) }
		out

	async saveLanguage(input: any)
		code := String(input.code ?? '').trim()
		return { ok: false, error: 'Use a language code such as en, de or pt-BR', field: 'code' } unless LANGUAGE_RE.test(code)
		knownL := KNOWN_LANGUAGES.find((k) => k.code.toLowerCase() === code.toLowerCase())
		fallback := String(input.fallback ?? '').trim()
		return { ok: false, error: 'A language cannot fall back to itself', field: 'fallback' } if fallback === code
		paths := {} as Record<string, string>
		for [k, v] of Object.entries(input.paths ?? {})
			s := slug v
			paths[k] = s if s
		await @repo.saveLanguage {
			code
			name: String(input.name || knownL?.name || code).trim()
			native: String(input.native || knownL?.native || '').trim()
			dir: input.dir === 'rtl' || input.dir === 'ltr' ? input.dir : (knownL?.dir ?? dirOf(code))
			fallback
			paths
			enabled: input.enabled !== false && input.enabled !== 'false'
			sort: Number(input.sort) || 0
		}
		@invalidate()
		{ ok: true, code }

	async deleteLanguage(code: string)
		cfg := await @config()
		used := cfg.markets.filter((m: any) => m.languages.some((l: any) => l.code === code))
		return { ok: false, error: `Remove it from ${used.map((m: any) => m.name).join(', ')} first` } if used.length
		n := await @repo.entriesIn code
		return { ok: false, error: `${n} entries are written in this language` } if n
		await @repo.deleteLanguage code
		@invalidate()
		{ ok: true }

	allMarkets()
		@repo.markets()

	market(id: number)
		@repo.market id

	// Validates the whole set, so two localisations can never claim the
	// same URLs.
	async saveMarket(id: number | null, input: any)
		known := new Set((await @repo.languages()).map((l: any) => l.code))
		code := slug input.code
		return { ok: false, error: 'Give it a short code (letters, digits, dashes)', field: 'code' } unless code
		name := String(input.name ?? '').trim() || code
		mode := ['path', 'subdomain', 'domain'].includes(input.mode) ? input.mode : 'path'
		prefix := mode === 'path' ? slug(input.prefix) : ''
		host := mode === 'path' ? '' : String(input.host ?? '').trim().toLowerCase().replace(/^https?:\/\//, '').replace(/\/.*$/, '')
		return { ok: false, error: 'Enter the host name, e.g. de.example.com or example.de', field: 'host' } if mode !== 'path' && !/^[a-z0-9.-]+\.[a-z]{2,}$/.test(host)
		langs := [] as any[]
		for l of (input.languages ?? [])
			lc := String(l.code ?? l).trim()
			continue unless lc
			return { ok: false, error: `Add the language ${lc} under Languages first`, field: 'languages' } unless known.has(lc)
			seg := slug(l.segment) || defaultSegment(lc)
			return { ok: false, error: `${lc} is listed twice`, field: 'languages' } if langs.some((x) => x.code === lc)
			return { ok: false, error: `Two languages use the URL segment "${seg}"`, field: 'languages' } if langs.some((x) => x.segment === seg)
			langs.push { code: lc, segment: seg }
		return { ok: false, error: 'Pick at least one language', field: 'languages' } unless langs.length
		defaultLanguage := langs.some((l) => l.code === input.defaultLanguage) ? input.defaultLanguage : langs[0].code
		// Forms post a hidden "false" before each switch: the last value wins.
		truthy := (raw: any) =>
			v := Array.isArray(raw) ? raw[raw.length - 1] : raw
			v === true || v === 'on' || v === 'true' || v === '1' || v === 1
		countries := (Array.isArray(input.countries) ? input.countries : String(input.countries ?? '').split(/[\s,]+/)).map((x: any) => String(x).trim().toUpperCase()).filter((x: string) => /^[A-Z]{2}$/.test(x))
		next := { code, name, mode, prefix, host, languages: langs, defaultLanguage, prefixDefault: truthy(input.prefixDefault), countries, contentFallback: truthy(input.contentFallback), isPrimary: truthy(input.isPrimary), enabled: input.enabled === undefined ? true : truthy(input.enabled), sort: Number(input.sort) || 0 }
		others := (await @repo.markets()).filter((m: any) => m.id !== id)
		return { ok: false, error: 'Another localisation uses that code', field: 'code' } if others.some((m: any) => m.code === code)
		if mode === 'path'
			if !prefix && others.some((m: any) => m.mode === 'path' && !m.prefix)
				return { ok: false, error: 'Only one sub-folder localisation can live at the root; give this one a prefix', field: 'prefix' }
			if prefix && others.some((m: any) => m.mode === 'path' && m.prefix === prefix)
				return { ok: false, error: 'Another localisation uses that prefix', field: 'prefix' }
			root := mode === 'path' && !prefix ? next : others.find((m: any) => m.mode === 'path' && !m.prefix)
			if root
				segs := new Set(root.languages.map((l: any) => l.segment))
				clash := [prefix, ...others.filter((m: any) => m.mode === 'path' && m.prefix).map((m: any) => m.prefix)].filter((p) => p && segs.has(p))
				return { ok: false, error: `"${clash[0]}" is both a localisation prefix and a language segment at the root`, field: 'prefix' } if clash.length
		else
			return { ok: false, error: 'Another localisation uses that host', field: 'host' } if others.some((m: any) => m.host === host)
		hadPrimary := others.some((m: any) => m.isPrimary)
		next.isPrimary = true unless hadPrimary
		// The primary localisation always answers (x-default, the admin's host).
		next.enabled = true if next.isPrimary
		if !next.isPrimary && id && (await @repo.market(id))?.isPrimary
			next.isPrimary = true
		savedId := await @repo.saveMarket id, next
		await @repo.clearPrimary(savedId) if next.isPrimary
		@invalidate()
		{ ok: true, id: savedId }

	async deleteMarket(id: number)
		m := await @repo.market id
		return { ok: false, error: 'Not found' } unless m
		return { ok: false, error: 'Make another localisation primary first' } if m.isPrimary
		await @repo.deleteMarket id
		@invalidate()
		{ ok: true }

	// Where each localisation and language lives, for the admin overview.
	async map()
		cfg := await @config()
		cfg.markets.map (m: any) => ({
			...m
			urls: m.languages.map((l: any) => ({ code: l.code, url: baseFor(cfg, m, l.code).origin + (basePath(m, l.code) || '/'), hreflang: hreflangs(m, l.code) }))
		})
