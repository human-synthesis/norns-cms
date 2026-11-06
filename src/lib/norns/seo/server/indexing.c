import type { Container } from '@human-synthesis/norns/server'
import type { SeoRepo } from './repo'
import { cms } from '../../content/server/public'
import { settings, envVar, siteOrigin, currentEvent, backgroundTask } from '../../settings/server/public'
import { i18n } from '../../i18n/server/public'
import { googleAccessToken } from './google.js'

// Search engine notifications.
//
// IndexNow (Bing — and through Bing ChatGPT search and Copilot — Yandex,
// Seznam, Naver, Yep): on demand (an entry, a list of URLs, the whole
// site) and automatically per content type for new, updated and removed
// entries, as set in the rules. Queued URLs are sent within seconds,
// after the response (waitUntil). Google does not take part in IndexNow and retired its
// sitemap ping in 2023; it gets the sitemap through the Search Console
// API instead (GOOGLE_SERVICE_ACCOUNT), from the cron at most every half
// hour when something changed. Scheduled entries going live are caught
// by the same cron.

INDEXNOW := 'https://api.indexnow.org/indexnow'
BATCH := 10000

INDEXNOW_STATUS: Record<number, string> := {
	200: 'Submitted'
	202: 'Accepted; the key is being checked'
	400: 'Bad request'
	403: 'Key not valid: the key file is not reachable on the host'
	422: 'URLs do not belong to the host'
	429: 'Too many requests; queued again'
}

// Only real public hosts are announced: never dev, previews or workers.dev.
export announceable := (url: string) =>
	try
		u := new URL(url)
		host := u.hostname
		u.protocol === 'https:' && host.includes('.') && !/^(localhost|127\.|0\.0\.0\.0|10\.|192\.168\.)/.test(host) && !/\.(local|test|workers\.dev|pages\.dev)$/.test(host)
	catch
		false

KINDS := ['new', 'updated', 'removed']

live := (e: any) => e?.status === 'published' && (!e.published_at || e.published_at <= Date.now())

sleep := (ms: number) => new Promise((r) => setTimeout(r, ms))

export class IndexingService
	repo: SeoRepo
	c: Container
	constructor(@repo: SeoRepo, @c: Container)

	async config()
		seo := await settings(@c).get 'seo'
		{
			indexNow: (await @rules()).enabled
			gscProperty: String(seo.gscProperty ?? '').trim()
			google: !!envVar('GOOGLE_SERVICE_ACCOUNT')
		}

	// IndexNow on/off and, per searchable content type, which changes are
	// sent automatically: { enabled, auto: { <type>: { label, new,
	// updated, removed } } }. Everything is on until changed.
	async rules()
		saved .= {} as any
		try
			saved = JSON.parse((await @repo.state('indexnow_rules')) ?? '{}') ?? {}
		catch
			saved = {}
		auto := {} as Record<string, any>
		for ct of cms(@c).types()
			continue if ct.seo === false
			mine := saved.auto?.[ct.id] ?? {}
			auto[ct.id] = { label: ct.labelPlural, new: mine.new ?? true, updated: mine.updated ?? true, removed: mine.removed ?? true }
		{ enabled: saved.enabled ?? true, auto, comments: saved.comments ?? false }

	// `input` like rules(); types and kinds left out keep their value.
	async saveRules(input: any)
		current := await @rules()
		auto := {} as Record<string, any>
		for [id, rule] of Object.entries(current.auto)
			given := input?.auto?.[id] ?? {}
			auto[id] = {} as Record<string, boolean>
			for kind of KINDS
				auto[id][kind] = kind in given ? !!given[kind] : (rule as any)[kind]
		enabled := input && 'enabled' in input ? !!input.enabled : current.enabled
		comments := input && 'comments' in input ? !!input.comments : current.comments
		await @repo.setState 'indexnow_rules', JSON.stringify({ enabled, auto, comments })
		@rules()

	// The IndexNow key, made on first use and served at /<key>.txt on
	// every host.
	async key()
		bytes := crypto.getRandomValues new Uint8Array 16
		@repo.initState 'indexnow_key', [...bytes].map((b) => b.toString(16).padStart(2, '0')).join('')

	// `/<32 hex>.txt` → the key file body, when it is the key.
	async keyFile(path: string)
		m := /^\/([0-9a-f]{32})\.txt$/.exec(path)
		return null unless m
		key := await @repo.state 'indexnow_key'
		key === m[1] ? key : null

	// Every public URL of an entry: one per localisation showing its
	// language, plus the type's list page and, for the home page, the
	// language home.
	async urlsOf(ct: any, entry: any)
		return [] unless ct && ct.seo !== false && entry && entry.visibility !== 'members'
		home := Number((await settings(@c).get('site')).homePage) || 0
		isHome := home > 0 && (entry.id === home || entry.translation_group === home)
		section := ct.listable ? cms(@c).sectionPath(ct, entry.locale) : ''
		out := [] as string[]
		for place of cms(@c).placesOf(entry.locale)
			continue if entry.markets?.length && place.market && !entry.markets.includes(place.market.code)
			root := (place.origin || siteOrigin()) + place.base
			out.push root + cms(@c).pathIn(ct, entry)
			out.push root + section if section
			out.push root + '/' if isHome
		out

	// The language homes on every host.
	homes()
		out := [] as string[]
		for code of i18n(@c).contentLanguages()
			for place of i18n(@c).placesOf(code)
				out.push (place.origin || siteOrigin()) + place.base + '/'
		out

	origins()
		[...new Set([siteOrigin(), ...@homes().map((u) => new URL(u).origin)])]

	// Content hook. `now` (kind new | updated) counts when it is live;
	// `gone` is an entry whose URLs were live and are removed or moved.
	// The sitemap is always marked changed for Google; IndexNow follows
	// the type's rules. Never throws.
	async changed(ct: any, change: { now?: any, kind?: string, gone?: any })
		try
			current := change.now && live(change.now) ? await @urlsOf(ct, change.now) : []
			gone := change.gone ? await @urlsOf(ct, change.gone) : []
			await @sitemapChanged [...current, ...gone]
			rule := (await @rules()).auto[ct?.id]
			send := [...(rule?.[change.kind ?? 'updated'] ? current : []), ...(rule?.removed ? gone : [])]
			await @announce send
		catch e
			console.error 'seo.indexing', e
		return

	// A comment was approved: the entry's page changed (when the rules
	// say so). Never throws.
	async commented(ct: any, entry: any)
		try
			await @announce(await @urlsOf(ct, entry)) if (await @rules()).comments && live(entry)
		catch e
			console.error 'seo.indexing', e
		return

	async sitemapChanged(urls: string[])
		return unless urls.some(announceable) && envVar('GOOGLE_SERVICE_ACCOUNT')
		await @repo.setState 'google_dirty', '1'
		return

	async announce(urls: string[], opts: { flush?: boolean } = {})
		list := [...new Set(urls)].filter(announceable)
		return 0 unless list.length
		await @sitemapChanged list
		return 0 unless (await @rules()).enabled
		await @repo.queue list
		await @schedule() unless opts.flush === false
		list.length

	// Send the queue after the response (once per request); without
	// waitUntil (Bun, tests) send it now.
	async schedule()
		event := currentEvent()
		later := backgroundTask()
		if later
			return if event?.locals?.indexingScheduled
			event.locals.indexingScheduled = true if event?.locals
			later sleep(1000).then(=> @flush()).catch((e) => console.error('seo.indexing', e))
			return
		await @flush()
		return

	async flush()
		urls := await @repo.takeQueue BATCH
		return { sent: 0, results: [] } unless urls.length
		key := await @key()
		byHost := new Map<string, string[]>()
		for u of urls
			host := new URL(u).host
			byHost.set host, [...(byHost.get(host) ?? []), u]
		results := [] as any[]
		for [host, list] of byHost
			results.push await @submitIndexNow(host, list, key)
		{ sent: urls.length, results }

	async submitIndexNow(host: string, urls: string[], key: string)
		status .= 0
		message .= ''
		try
			res := await fetch INDEXNOW, {
				method: 'POST'
				headers: { 'content-type': 'application/json; charset=utf-8' }
				body: JSON.stringify({ host, key, keyLocation: `https://${host}/${key}.txt`, urlList: urls })
			}
			status = res.status
			message = (await res.text()).trim().slice(0, 300) unless res.ok
		catch e
			message = String((e as any)?.message ?? e).slice(0, 300)
		ok := status === 200 || status === 202
		// Rate limits, outages and network errors: try again next time.
		await @repo.queue urls if !ok && (status === 0 || status === 429 || status >= 500)
		await @repo.logIndexing 'indexnow', host, urls.length, ok, status || null, INDEXNOW_STATUS[status] ?? message
		{ engine: 'indexnow', host, urls: urls.length, ok, status }

	// Search Console: (re)submit /sitemap.xml of every host.
	async submitSitemaps()
		cfg := await @config()
		return { skipped: 'GOOGLE_SERVICE_ACCOUNT is not set' } unless cfg.google
		out := [] as any[]
		auth .= null as any
		try
			auth = await googleAccessToken envVar('GOOGLE_SERVICE_ACCOUNT')
		catch e
			message := String((e as any)?.message ?? e).slice(0, 300)
			await @repo.logIndexing 'google', '', 0, false, null, message
			return { ok: false, error: message }
		for origin of @origins()
			sitemap := `${origin}/sitemap.xml`
			continue unless announceable(sitemap)
			property := cfg.gscProperty || `${origin}/`
			status .= 0
			message .= ''
			try
				res := await fetch `https://www.googleapis.com/webmasters/v3/sites/${encodeURIComponent(property)}/sitemaps/${encodeURIComponent(sitemap)}`, {
					method: 'PUT'
					headers: { authorization: `Bearer ${auth.token}` }
				}
				status = res.status
				unless res.ok
					body := await res.json().catch(=> ({}))
					message = String(body?.error?.message ?? res.statusText).slice(0, 300)
					message = `${message} — add ${auth.email} as an owner of ${property} in Search Console` if status === 403
			catch e
				message = String((e as any)?.message ?? e).slice(0, 300)
			ok := status >= 200 && status < 300
			await @repo.logIndexing 'google', sitemap, 1, ok, status || null, ok ? `Submitted to ${property}` : message
			out.push { engine: 'google', sitemap, property, ok, status }
		await @repo.setState 'google_dirty', '0'
		await @repo.setState 'google_at', String(Date.now())
		{ ok: out.every((r) => r.ok), results: out }

	// Entries whose scheduled publish time passed since the last run.
	async catchUp()
		now := Date.now()
		since := Number(await @repo.state('scheduled_at')) || now
		urls := [] as string[]
		n .= 0
		for ct of cms(@c).types()
			continue if ct.seo === false
			for e of await cms(@c).list(ct.id, { status: 'published', order: 'published', limit: 200 })
				continue unless e.published_at && e.published_at > since && e.published_at <= now
				urls.push ...(await @urlsOf(ct, e))
				n++
		await @repo.setState 'scheduled_at', String(now)
		await @announce urls, { flush: false }
		n

	// Cron: scheduled entries, the IndexNow queue, Google when changed.
	async tick()
		out := { scheduled: await @catchUp(), indexNow: await @flush() } as Record<string, any>
		out.google = await @submitSitemaps() if (await @repo.state('google_dirty')) === '1'
		out

	// Admin: one entry now, whatever the rules say (IndexNow must be on).
	async submitEntry(ct: any, entry: any)
		return { sent: 0, results: [], reason: 'IndexNow is switched off' } unless (await @rules()).enabled
		return { sent: 0, results: [], reason: 'Only published, public entries can be sent' } unless live(entry) && entry.visibility !== 'members'
		urls := (await @urlsOf(ct, entry)).filter(announceable)
		return { sent: 0, results: [], reason: 'This site is not on a public https host' } unless urls.length
		await @announce urls, { flush: false }
		@flush()

	// Admin: URLs typed in by hand; only this site's hosts.
	async submitUrls(input: string[] | string)
		return { sent: 0, results: [], skipped: [], reason: 'IndexNow is switched off' } unless (await @rules()).enabled
		list := (Array.isArray(input) ? input : String(input ?? '').split(/\s+/)).map((u) => String(u).trim()).filter(Boolean)
		origins := @origins()
		mine := list.filter (u) =>
			try
				announceable(u) && origins.includes(new URL(u).origin)
			catch
				false
		skipped := list.filter((u) => !mine.includes(u))
		await @announce mine, { flush: false }
		{ ...(await @flush()), skipped }

	// Admin: announce every public, indexable URL now.
	async submitAll()
		return { queued: 0, sent: 0, results: [], reason: 'IndexNow is switched off' } unless (await @rules()).enabled
		urls := @homes()
		for ct of cms(@c).types()
			continue if ct.seo === false
			for e of await cms(@c).list(ct.id, { status: 'published', visibility: 'public', limit: 5000 })
				continue if e.data?.seoNoindex || e.data?.seoCanonical
				urls.push ...(await @urlsOf(ct, e))
		queued := await @announce urls, { flush: false }
		{ queued, ...(await @flush()) }

	async status()
		cfg := await @config()
		key := await @repo.state 'indexnow_key'
		origin := siteOrigin()
		{
			...cfg
			live: announceable(`${origin}/`)
			origin
			keyUrl: key ? `${origin}/${key}.txt` : null
			queued: await @repo.queued()
			googleAt: Number(await @repo.state('google_at')) || null
			googlePending: (await @repo.state('google_dirty')) === '1'
			rules: await @rules()
			log: await @repo.indexingLog(30)
		}
