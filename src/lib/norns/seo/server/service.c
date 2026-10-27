import type { Container } from '@human-synthesis/norns/server'
import type { SeoRepo } from './repo'
import { cms } from '../../content/server/public'
import { settings, siteOrigin } from '../../settings/server/public'
import { articles } from '../../articles/server/public'
import { AI_BOTS, botField, seoChecks } from '../shared/fields'
import { xmlEscape, iso, rfc822, ldJson, plain } from './xml.js'
import { i18n } from '../../i18n/server/public'
import { getScope } from '@human-synthesis/norns/server'

// Absolute URL on the current request's host (its localisation's origin).
abs := (path: string) =>
	return path if /^https?:\/\//.test(path)
	origin := getScope()?.event?.locals?.locale?.origin ?? siteOrigin()
	`${origin}${path.startsWith('/') ? path : '/' + path}`

locale := => getScope()?.event?.locals?.locale ?? { language: 'en', base: '', market: null, chain: ['en'] }
ogLocale := (code: string, countries: string[] = []) =>
	[l, r] := code.split('-')
	region := r ?? (countries.length === 1 ? countries[0] : '')
	region ? `${l}_${region.toUpperCase()}` : l

DAY := 24 * 60 * 60 * 1000

export class SeoService
	repo: SeoRepo
	c: Container
	constructor(@repo: SeoRepo, @c: Container)

	async config()
		site := await settings(@c).localized 'site'
		seo := await settings(@c).localized 'seo'
		{ site, seo }

	// --- head tags -----------------------------------------------------------

	formatTitle(title: string, site: any, seo: any)
		return site.title unless title
		return title if title === site.title
		String(seo.titleTemplate || '%s · %site').replace('%s', title).replace('%site', site.title)

	async mediaUrl(id: any)
		n := Number(id)
		return null unless n > 0
		m := (await cms(@c).lookupMedia([n]))[n]
		m ? { url: abs(m.url), width: m.width, height: m.height, alt: m.alt } : null

	organization(site: any, seo: any, logo: any)
		org := {
			'@type': seo.orgType === 'Person' ? 'Person' : (seo.orgType === 'LocalBusiness' ? (seo.bizType || 'LocalBusiness') : 'Organization')
			'@id': abs('/#organization')
			name: seo.orgName || site.title
			url: abs('/')
		} as Record<string, any>
		org.logo = logo.url if logo && org['@type'] !== 'Person'
		org.image = logo.url if logo && org['@type'] === 'Person'
		org.sameAs = seo.orgSameAs if seo.orgSameAs?.length
		Object.assign(org, @businessFields(seo)) if seo.orgType === 'LocalBusiness'
		org

	hasBusiness(seo: any)
		!!(seo.bizStreet || seo.bizCity || seo.bizPhone || (seo.bizLat && seo.bizLng))

	businessFields(seo: any)
		out := {} as Record<string, any>
		if seo.bizStreet || seo.bizCity
			out.address = { '@type': 'PostalAddress', streetAddress: seo.bizStreet || undefined, addressLocality: seo.bizCity || undefined, addressRegion: seo.bizRegion || undefined, postalCode: seo.bizPostal || undefined, addressCountry: seo.bizCountry || undefined }
		if seo.bizLat && seo.bizLng
			out.geo = { '@type': 'GeoCoordinates', latitude: Number(seo.bizLat), longitude: Number(seo.bizLng) }
		out.telephone = seo.bizPhone if seo.bizPhone
		out.email = seo.bizEmail if seo.bizEmail
		out.openingHours = seo.bizHours if seo.bizHours?.length
		out.priceRange = seo.bizPriceRange if seo.bizPriceRange
		out.areaServed = seo.bizArea if seo.bizArea
		out

	// Location block / settings → display data.
	async location()
		{ seo } := await @config()
		return null unless @hasBusiness(seo)
		lines := [seo.bizStreet, [seo.bizPostal, seo.bizCity].filter(Boolean).join(' '), seo.bizRegion, seo.bizCountry].filter(Boolean)
		map := if seo.bizLat && seo.bizLng
			`https://www.openstreetmap.org/?mlat=${seo.bizLat}&mlon=${seo.bizLng}#map=16/${seo.bizLat}/${seo.bizLng}`
		else if lines.length
			`https://www.openstreetmap.org/search?query=${encodeURIComponent(lines.join(', '))}`
		else
			null
		{ name: seo.orgName || '', lines, phone: seo.bizPhone, email: seo.bizEmail, hours: seo.bizHours ?? [], area: seo.bizArea, map }

	// Defaults for every public page (site layout): verification tokens,
	// the site-wide JSON-LD and the feed link.
	async siteHead()
		{ site, seo } := await @config()
		logo := await @mediaUrl seo.orgLogo
		org := @organization site, seo, logo
		website := { '@type': 'WebSite', '@id': abs('/#website'), name: site.title, url: abs('/'), publisher: { '@id': org['@id'] } } as Record<string, any>
		website.description = site.tagline if site.tagline
		loc := locale()
		website.inLanguage = loc.language
		{
			siteName: site.title
			locale: ogLocale(loc.language, loc.market?.countries ?? [])
			twitter: seo.twitter || ''
			google: seo.googleVerification || ''
			bing: seo.bingVerification || ''
			feed: abs(`${loc.base}/feed.xml`)
			jsonLd: ldJson({ '@context': 'https://schema.org', '@graph': [org, website] })
		}

	// Head for one resolved entry (result of cms.resolve + presentation).
	async forEntry(res: any, extra: Record<string, any> = {})
		{ site, seo } := await @config()
		{ ct, entry } := res
		d := entry.data
		path := cms(@c).urlFor ct, entry
		alternates := await @alternates(ct, entry, res.alternates ?? [])
		title := d.seoTitle || entry.title
		description := plain(d.seoDescription || d.summary || d.excerpt || (res.html?.body ?? '') || seo.defaultDescription || site.tagline, 160)
		image := (await @mediaUrl(d.seoImage)) ?? (await @mediaUrl(d.cover)) ?? (await @mediaUrl(seo.defaultImage))
		noindex := !!d.seoNoindex || entry.status !== 'published' || (entry.published_at ?? 0) > Date.now()
		robots := [noindex ? 'noindex' : 'index', d.seoNofollow ? 'nofollow' : 'follow', 'max-image-preview:large'].join(', ')
		canonical := d.seoCanonical ? abs(d.seoCanonical) : (res.fallbackFrom ? cms(@c).absoluteUrlFor(ct, entry) : abs(path))
		isArticle := ct.id === 'article'
		schemaType := d.seoSchema || ct.schemaType || (isArticle ? 'BlogPosting' : 'WebPage')
		graph := [] as any[]
		crumbs := [{ name: site.title, url: abs('/') }]
		if ct.basePath
			crumbs.push { name: ct.labelPlural, url: abs(ct.basePath) }
		if extra.category
			crumbs.push { name: extra.category.name, url: abs(extra.category.url) }
		crumbs.push { name: entry.title, url: canonical }
		graph.push { '@type': 'BreadcrumbList', itemListElement: crumbs.map((cr, i) => ({ '@type': 'ListItem', position: i + 1, name: cr.name, item: cr.url })) }
		main := {
			'@type': schemaType
			'@id': `${canonical}#main`
			url: canonical
			name: title
			headline: title.slice(0, 110)
			description
			inLanguage: entry.locale ?? locale().language
			isPartOf: { '@id': abs('/#website') }
			datePublished: entry.published_at ? iso(entry.published_at) : undefined
			dateModified: iso(entry.updated_at)
		} as Record<string, any>
		main.image = image.url if image
		if ['Article', 'BlogPosting', 'NewsArticle', 'HowTo'].includes(schemaType)
			main.publisher = { '@id': abs('/#organization') }
			main.mainEntityOfPage = canonical
			main.wordCount = d.words if d.words
			main.articleSection = extra.category.name if extra.category
			main.keywords = d.tags.join(', ') if d.tags?.length
			main.abstract = (d.keyFacts?.length ? d.keyFacts.join(' ') : d.summary) || undefined
			if extra.authors?.length
				main.author = extra.authors.map (a) => ({ '@type': 'Person', name: a.name, url: a.url ? abs(a.url) : undefined })
			else
				main.author = { '@id': abs('/#organization') }
			main.citation = extra.sources.filter((s) => s.url).map((s) => ({ '@type': 'CreativeWork', name: s.title, url: s.url })) if extra.sources?.length
			main.isAccessibleForFree = entry.visibility !== 'members'
		graph.push main
		faqs := (res.blocks ?? []).filter((b: any) => b.type === 'faq')
		if faqs.length
			questions := [] as any[]
			for b of faqs
				for item, i of (b.props.items ?? [])
					answer := b.html?.items?.[i]?.answer?.html ?? item.answer ?? ''
					questions.push { '@type': 'Question', name: item.question, acceptedAnswer: { '@type': 'Answer', text: plain(answer, 1000) } }
			graph.push { '@type': 'FAQPage', '@id': `${canonical}#faq`, mainEntity: questions } if questions.length
		hasLocation := (res.blocks ?? []).some((b: any) => b.type === 'location')
		if hasLocation && @hasBusiness(seo) && seo.orgType !== 'LocalBusiness'
			graph.push { '@type': seo.bizType || 'LocalBusiness', name: seo.orgName || site.title, url: abs('/'), ...@businessFields(seo) }
		twin := !d.seoNoLlms && !noindex && entry.visibility === 'public' && !res.fallbackFrom ? abs(`${path === '/' ? '/index' : path}.md`) : null
		main.workTranslation = alternates.filter((a) => a.locale !== entry.locale && a.hreflang !== 'x-default').map((a) => ({ '@id': `${a.href}#main`, inLanguage: a.locale })) if alternates.length > 1
		{
			title: @formatTitle(title, site, seo)
			description
			canonical
			robots
			type: isArticle ? 'article' : 'website'
			image
			published: entry.published_at ? iso(entry.published_at) : null
			modified: iso(entry.updated_at)
			section: extra.category?.name ?? null
			tags: d.tags ?? []
			markdown: twin
			alternates: res.fallbackFrom ? [] : alternates
			ogLocale: ogLocale(entry.locale ?? locale().language, locale().market?.countries ?? [])
			ogAlternates: [...new Set(alternates.filter((a) => a.hreflang !== 'x-default' && a.locale !== entry.locale).map((a) => ogLocale(a.locale)))]
			jsonLd: ldJson({ '@context': 'https://schema.org', '@graph': graph })
		}

	// hreflang alternates: every published translation × every localisation
	// offering its language (and where the entry is available), plus
	// x-default → the primary localisation's default language (else the
	// entry itself). Only emitted when there is more than one.
	async alternates(ct: any, entry: any, translations: any[])
		list := translations.filter((t) => t.visibility === 'public')
		out := [] as { hreflang: string, href: string, locale: string }[]
		for t of list
			for place of i18n(@c).placesOf(t.locale)
				continue if t.markets?.length && place.market && !t.markets.includes(place.market.code)
				for code of place.hreflang
					out.push { hreflang: code, href: `${place.origin}${place.base}${t.path}`, locale: t.locale }
		return [] unless out.length > 1
		primary := i18n(@c).primary()
		def := out.find((a) => a.locale === primary?.defaultLanguage && a.href.startsWith(i18n(@c).link(a.locale, primary.code).origin)) ?? out[0]
		seen := new Set<string>()
		uniq := [] as any[]
		for a of out
			k := a.hreflang + a.href
			continue if seen.has k
			seen.add k
			uniq.push a
		[...uniq, { hreflang: 'x-default', href: def.href, locale: def.locale }]

	// Head for archive / listing / utility pages.
	async forPage(input: { title?: string, description?: string, path: string, noindex?: boolean, crumbs?: { name: string, url: string }[], alternates?: any[] })
		{ site, seo } := await @config()
		image := await @mediaUrl seo.defaultImage
		canonical := abs input.path
		graph := [] as any[]
		if input.crumbs?.length
			list := [{ name: site.title, url: '/' }, ...input.crumbs]
			graph.push { '@type': 'BreadcrumbList', itemListElement: list.map((cr, i) => ({ '@type': 'ListItem', position: i + 1, name: cr.name, item: abs(cr.url) })) }
		graph.push { '@type': input.path === '/' ? 'WebPage' : 'CollectionPage', url: canonical, name: input.title || site.title, isPartOf: { '@id': abs('/#website') } }
		{
			title: input.title ? @formatTitle(input.title, site, seo) : site.title
			description: plain(input.description || seo.defaultDescription || site.tagline, 160)
			canonical
			robots: input.noindex ? 'noindex, follow' : 'index, follow, max-image-preview:large'
			type: 'website'
			image
			alternates: input.alternates ?? []
			ogLocale: ogLocale(locale().language, locale().market?.countries ?? [])
			jsonLd: ldJson({ '@context': 'https://schema.org', '@graph': graph })
		}

	// Language homes as alternates (home page, list pages): one per language
	// base, with the same path inside.
	languageAlternates(rest: string = '/')
		out := [] as any[]
		for code of i18n(@c).contentLanguages()
			for place of i18n(@c).placesOf(code)
				for h of place.hreflang
					out.push { hreflang: h, href: `${place.origin}${place.base}${rest === '/' ? '/' : rest}`, locale: code }
		return [] unless out.length > 1
		p := i18n(@c).primary()
		def := out.find((a) => a.locale === p?.defaultLanguage) ?? out[0]
		[...out, { hreflang: 'x-default', href: def.href, locale: def.locale }]

	// --- crawl surface ---------------------------------------------------------

	// Public, indexable entries of a type.
	async indexable(typeId: string, limit = 50000)
		{ entries } := await cms(@c).query typeId, { limit }
		entries.filter((e) => !e.data.seoNoindex && !e.data.seoCanonical)

	// The locale bases served by this host (path localisations live on the
	// primary host; sub-domain / domain localisations on their own).
	basesOnHost()
		host := (getScope()?.event?.url?.host ?? '').toLowerCase()
		out := [] as any[]
		for code of i18n(@c).contentLanguages()
			for place of i18n(@c).placesOf(code)
				placeHost := place.origin.replace(/^https?:\/\//, '').toLowerCase()
				isPathMarket := place.market?.mode === 'path' || !place.market?.host
				there := placeHost === host || (isPathMarket && !i18n(@c).markets().some((m: any) => m.mode !== 'path' && m.host === host))
				out.push { code, base: place.base, origin: place.origin, market: place.market } if there
		out

	async sitemapIndex()
		parts := [] as string[]
		for b of @basesOnHost()
			for ct of cms(@c).types()
				parts.push `<sitemap><loc>${xmlEscape(abs(`${b.base}/sitemap-${ct.id}.xml`))}</loc></sitemap>`
			parts.push `<sitemap><loc>${xmlEscape(abs(`${b.base}/sitemap-archives.xml`))}</loc></sitemap>`
		`<?xml version="1.0" encoding="UTF-8"?>\n<sitemapindex xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">${parts.join('')}</sitemapindex>\n`

	urlset(urls: { loc: string, lastmod?: string, image?: string | null, alternates?: any[] }[])
		body := urls.map((u) => `<url><loc>${xmlEscape(u.loc)}</loc>${u.lastmod ? `<lastmod>${u.lastmod}</lastmod>` : ''}${(u.alternates ?? []).map((a) => `<xhtml:link rel="alternate" hreflang="${xmlEscape(a.hreflang)}" href="${xmlEscape(a.href)}"/>`).join('')}${u.image ? `<image:image><image:loc>${xmlEscape(u.image)}</image:loc></image:image>` : ''}</url>`).join('')
		`<?xml version="1.0" encoding="UTF-8"?>\n<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9" xmlns:xhtml="http://www.w3.org/1999/xhtml" xmlns:image="http://www.google.com/schemas/sitemap-image/1.1">${body}</urlset>\n`

	// One locale's sitemap of a type (the request's locale).
	async sitemapFor(typeId: string)
		loc := locale()
		if typeId === 'archives'
			urls := [{ loc: abs(`${loc.base}/`), alternates: @languageAlternates('/') }] as any[]
			for ct of cms(@c).types()
				section := cms(@c).sectionPath ct, loc.language
				urls.push { loc: abs(`${loc.base}${section}`) } if section && ct.listable
			for tax of ['category', 'tag']
				for t of await articles(@c).terms(tax)
					urls.push { loc: abs(articles(@c).termUrl(t, loc.language)) } if t.count > 0
			return @urlset urls
		ct := cms(@c).getType typeId
		return null unless ct
		entries := await @indexable typeId
		covers := await cms(@c).lookupMedia entries.map((e) => Number(e.data.cover)).filter((n) => n > 0)
		urls := [] as any[]
		for e of entries
			alts := await @alternates ct, e, await cms(@c).alternatesOf(ct, e)
			urls.push { loc: abs(cms(@c).urlFor(ct, e)), lastmod: iso(e.updated_at), image: covers[e.data.cover] ? abs(covers[e.data.cover].url) : null, alternates: alts }
		@urlset urls

	async robots()
		{ seo } := await @config()
		lines := ['User-agent: *', 'Allow: /', 'Disallow: /admin', 'Disallow: /account', 'Disallow: /api/', 'Disallow: /login', '']
		blocked := AI_BOTS.filter((b) => seo[botField(b.id)] === false)
		for b of blocked
			lines.push `User-agent: ${b.id}`, 'Disallow: /', ''
		lines.push String(seo.robotsExtra).trim(), '' if String(seo.robotsExtra ?? '').trim()
		lines.push `Sitemap: ${abs('/sitemap.xml')}`
		lines.join('\n') + '\n'

	twinable(e: any)
		e.visibility === 'public' && !e.data.seoNoLlms && !e.data.seoNoindex

	async llms()
		{ site, seo } := await @config()
		loc := locale()
		out := [`# ${site.title}`, '', `> ${plain(seo.llmsSummary || site.tagline || seo.defaultDescription || site.title, 400)}`, '']
		others := [] as string[]
		for code of i18n(@c).contentLanguages()
			continue if code === loc.language
			l := i18n(@c).link(code)
			others.push `- [${i18n(@c).language(code).native || code}](${l.origin}${l.base}/llms.txt)`
		if others.length
			out.push `This file covers the ${i18n(@c).language(loc.language).name || loc.language} site. Other languages:`, '', ...others, ''
		for ct of cms(@c).types()
			entries := (await @indexable(ct.id, 500)).filter((e) => @twinable(e))
			continue unless entries.length
			out.push `## ${ct.labelPlural}`, ''
			for e of entries
				path := cms(@c).urlFor ct, e
				desc := plain(e.data.summary || e.data.seoDescription || e.data.excerpt || '', 200)
				out.push `- [${e.title}](${abs(path + '.md')})${desc ? ': ' + desc : ''}`
			out.push ''
		if seo.llmsFull !== false
			out.push '## Optional', '', `- [Everything in one file](${abs(`${loc.base}/llms-full.txt`)})`, ''
		out.join('\n')

	async llmsFull()
		{ site, seo } := await @config()
		return null if seo.llmsFull === false
		parts := [`# ${site.title}`, '', plain(seo.llmsSummary || site.tagline || '', 400), '']
		size .= 0
		for ct of cms(@c).types()
			for e of (await @indexable(ct.id, 500)).filter((x) => @twinable(x))
				md := await @markdownFor ct, e
				size += md.length
				break if size > 2_000_000
				parts.push '---', '', md, ''
		parts.join('\n')

	// The Markdown twin of an entry: front matter plus its source Markdown
	// (blocks pages are flattened to Markdown).
	async markdownFor(ct: any, e: any)
		d := e.data
		path := cms(@c).urlFor ct, e
		front := ['---', `title: ${JSON.stringify(e.title)}`, `url: ${abs(path)}`]
		front.push `published: ${iso(e.published_at)}` if e.published_at
		front.push `updated: ${iso(e.updated_at)}`
		front.push `summary: ${JSON.stringify(plain(d.summary, 400))}` if d.summary
		front.push `tags: ${JSON.stringify(d.tags)}` if d.tags?.length
		front.push '---', ''
		body := [`# ${e.title}`, ''] as string[]
		body.push `> ${plain(d.summary, 600)}`, '' if d.summary
		if d.keyFacts?.length
			body.push '## Key facts', '', ...d.keyFacts.map((f: string) => `- ${f}`), ''
		if d.layout === 'blocks'
			for b of (d.blocks ?? [])
				p := b.props ?? {}
				if b.type === 'hero' || b.type === 'cta'
					body.push `## ${p.title}`, '' if p.title
					body.push p.text, '' if p.text
				else if b.type === 'markdown'
					body.push p.body, '' if p.body
				else if b.type === 'columns'
					body.push `## ${p.title}`, '' if p.title
					for col of (p.columns ?? [])
						body.push `### ${col.title}`, '' if col.title
						body.push col.body, '' if col.body
				else if b.type === 'faq'
					body.push `## ${p.title || 'FAQ'}`, ''
					for item of (p.items ?? [])
						body.push `### ${item.question}`, '', item.answer ?? '', ''
		else if d.body
			body.push String(d.body), ''
		if d.sources?.length
			body.push '## Sources', '', ...articles(@c).sources(e).map((s: any) => s.url ? `- [${s.title}](${s.url})` : `- ${s.title}`), ''
		[...front, ...body].join('\n').replace(/media:(\d+)/g, (_m: string, id: string) => abs(`/media/by-id/${id}`))

	// `path` is inside the request's locale base.
	async twin(path: string)
		p .= path
		if p === '/'
			home := await @homeEntry()
			return null unless home
			p = `/${home.slug}`
		res := await cms(@c).resolve p, {}
		return null unless res && res.kind === 'entry' && !res.locked && @twinable(res.entry)
		await @markdownFor res.ct, res.entry

	// The home page of the request's language: the chosen page's
	// translation.
	async homeEntry()
		{ site } := await @config()
		return null unless site.homePage
		home := await cms(@c).byId Number(site.homePage)
		return null unless home
		lang := locale().language
		return home if home.locale === lang
		alts := await cms(@c).alternatesOf cms(@c).getType(home.type), home
		hit := alts.find((a) => a.locale === lang)
		hit ? { ...home, slug: hit.slug, locale: lang } : null

	async feed(opts: { termId?: number, title?: string, path?: string } = {})
		{ site } := await @config()
		loc := locale()
		list := await articles(@c).list { perPage: 30, termId: opts.termId }
		items := list.entries.map (e: any) =>
			html := e.rendered?.body?.html ?? ''
			[
				'<item>'
				`<title>${xmlEscape(e.title)}</title>`
				`<link>${xmlEscape(abs(e.url))}</link>`
				`<guid isPermaLink="true">${xmlEscape(abs(e.url))}</guid>`
				`<pubDate>${rfc822(e.published_at ?? e.updated_at)}</pubDate>`
				...(e.authors ?? []).map((a: any) => `<dc:creator>${xmlEscape(a.name)}</dc:creator>`)
				...(e.category ? [`<category>${xmlEscape(e.category.name)}</category>`] : [])
				`<description>${xmlEscape(e.data.summary || plain(html, 300))}</description>`
				`<content:encoded><![CDATA[${html.replace(/]]>/g, ']]]]><![CDATA[>')}]]></content:encoded>`
				'</item>'
			].join('')
		self := abs(opts.path ?? `${loc.base}/feed.xml`)
		[
			'<?xml version="1.0" encoding="UTF-8"?>'
			'<rss version="2.0" xmlns:atom="http://www.w3.org/2005/Atom" xmlns:content="http://purl.org/rss/1.0/modules/content/" xmlns:dc="http://purl.org/dc/elements/1.1/">'
			'<channel>'
			`<title>${xmlEscape(opts.title ? `${opts.title} · ${site.title}` : site.title)}</title>`
			`<link>${xmlEscape(abs(`${loc.base}${cms(@c).sectionPath(cms(@c).getType('article'), loc.language)}`))}</link>`
			`<description>${xmlEscape(site.tagline || site.title)}</description>`
			`<language>${xmlEscape(loc.language)}</language>`
			`<atom:link href="${xmlEscape(self)}" rel="self" type="application/rss+xml"/>`
			`<lastBuildDate>${rfc822(Date.now())}</lastBuildDate>`
			...items
			'</channel></rss>'
		].join('\n')

	// --- redirects -----------------------------------------------------------

	normalize(path: string)
		p .= String(path ?? '').trim()
		try
			p = new URL(p).pathname if /^https?:\/\//.test(p)
		catch
			p = p
		p = '/' + p unless p.startsWith('/')
		p = p.replace(/\/+$/, '') if p.length > 1
		p

	async redirectFor(path: string)
		@repo.redirectFor @normalize(path)

	redirects(q = '')
		@repo.redirects q

	async saveRedirect(id: number | null, input: { from: string, to: string, status: any })
		from := @normalize input.from
		status := [301, 302, 307, 308, 410].includes(Number(input.status)) ? Number(input.status) : 301
		to := status === 410 ? '' : String(input.to ?? '').trim()
		return { ok: false, error: 'Enter the old path', field: 'from' } if from === '/' && !input.from
		return { ok: false, error: 'Enter where it should go', field: 'to' } if status !== 410 && !to
		target := /^https?:\/\//.test(to) ? to : @normalize(to)
		return { ok: false, error: 'A redirect cannot point at itself', field: 'to' } if target === from
		onward := await @repo.redirectFor target
		return { ok: false, error: `That would loop: ${target} already redirects to ${onward.to_path}`, field: 'to' } if onward && @normalize(onward.to_path) === from
		if id
			await @repo.updateRedirect id, from, target, status
		else
			await @repo.upsertRedirect from, target, status, 'manual'
		await @repo.clearNotFound from
		{ ok: true }

	removeRedirect(id: number)
		@repo.removeRedirect id

	// CSV / lines: "from,to[,status]".
	async importRedirects(text: string)
		n .= 0
		errors := [] as string[]
		for line, i of String(text ?? '').split(/\r?\n/)
			continue unless line.trim() && !line.trim().startsWith('#')
			[from, to, status] := line.split(/[,\t;]/).map((s) => s.trim())
			r := await @saveRedirect null, { from, to, status: status || 301 }
			if r.ok
				n++
			else
				errors.push `Line ${i + 1}: ${r.error}`
		{ imported: n, errors }

	// Content hook: a published entry changed its URL.
	async slugChanged(fromUrl: string, toUrl: string)
		from := @normalize fromUrl
		to := @normalize toUrl
		return if from === to
		await @repo.removeFrom to
		await @repo.retarget from, to
		await @repo.upsertRedirect from, to, 301, 'auto'
		return

	hit(id: number)
		@repo.hit id

	log404(path: string, referrer: string)
		@repo.log404 path, referrer

	notFound()
		@repo.notFound()

	clearNotFound(path: string | null)
		@repo.clearNotFound path

	count404()
		@repo.count404()

	purge()
		@repo.purge404 Date.now() - 90 * DAY

	// --- report ----------------------------------------------------------------

	async report()
		{ site, seo } := await @config()
		rows := [] as any[]
		for ct of cms(@c).types()
			for e of await cms(@c).list(ct.id, { limit: 500 })
				issues := seoChecks { ...e.data, title: e.title, slug: e.slug }, { siteTitle: site.title, template: seo.titleTemplate }
				problems := issues.filter((i) => i.level !== 'info')
				rows.push { id: e.id, type: ct.id, typeLabel: ct.label, title: e.title, status: e.status, issues: problems } if problems.length
		rows
