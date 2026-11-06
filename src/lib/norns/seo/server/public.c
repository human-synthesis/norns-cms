import type { Container } from '@human-synthesis/norns/server'
import type { SeoService } from './service'
import type { IndexingService } from './indexing'
import { cms } from '../../content/server/public'
import { articles } from '../../articles/server/public'
import { i18n } from '../../i18n/server/public'

export { seoChecks, AI_BOTS } from '../shared/fields'

svc := (c: Container) => c.resolve('seo.service') as SeoService
ix := (c: Container) => c.resolve('seo.indexing') as IndexingService

export seo := (c: Container) => {
	siteHead: () => svc(c).siteHead()
	forEntry: (res: any, extra?: any) => svc(c).forEntry(res, extra)
	forPage: (input: any) => svc(c).forPage(input)
	sitemapIndex: () => svc(c).sitemapIndex()
	sitemapFor: (typeId: string) => svc(c).sitemapFor(typeId)
	robots: () => svc(c).robots()
	llms: () => svc(c).llms()
	llmsFull: () => svc(c).llmsFull()
	twin: (path: string) => svc(c).twin(path)
	feed: (opts?: any) => svc(c).feed(opts)
	homeEntry: () => svc(c).homeEntry()
	languageAlternates: (rest?: string) => svc(c).languageAlternates(rest)
	location: () => svc(c).location()
	redirects: (q?: string) => svc(c).redirects(q)
	saveRedirect: (id: number | null, input: any) => svc(c).saveRedirect(id, input)
	removeRedirect: (id: number) => svc(c).removeRedirect(id)
	importRedirects: (text: string) => svc(c).importRedirects(text)
	notFound: () => svc(c).notFound()
	clearNotFound: (path: string | null) => svc(c).clearNotFound(path)
	count404: () => svc(c).count404()
	probeSuggestions: () => svc(c).probeSuggestions()
	blockFromLog: (pattern: string, user?: any, note?: string) => svc(c).blockFromLog(pattern, user, note)
	report: () => svc(c).report()
	purge: () => svc(c).purge()
	indexingStatus: () => ix(c).status()
	submitAll: () => ix(c).submitAll()
	submitEntry: (ct: any, entry: any) => ix(c).submitEntry(ct, entry)
	submitUrls: (urls: string[] | string) => ix(c).submitUrls(urls)
	indexingRules: () => ix(c).rules()
	commentApproved: (ct: any, entry: any) => ix(c).commented(ct, entry)
	saveIndexingRules: (input: any) => ix(c).saveRules(input)
	submitSitemaps: () => ix(c).submitSitemaps()
	indexingTick: () => ix(c).tick()
}

SKIP := /^\/(admin|api|account|login|media|_app|cdn-cgi|reset|invite|verify|forms|comments)(\/|$)/
ASSET := /\.(js|css|map|png|jpe?g|gif|webp|avif|svg|ico|woff2?|ttf|txt|xml|json)$/i

text := (body: string, type: string, extra: Record<string, string> = {}) =>
	new Response(body, { headers: { 'content-type': `${type}; charset=utf-8`, 'cache-control': 'public, max-age=600', ...extra } })

// Redirects (before routing), the crawl outputs and the 404 log. Crawl
// outputs exist per locale — {base}/feed.xml, {base}/llms.txt,
// {base}/llms-full.txt, {base}/sitemap-<type>.xml, {base}/…/x.md — and per
// host — /robots.txt, /sitemap.xml, /<IndexNow key>.txt. Runs after
// localeHandle (needs locals.locale).
export seoHandle := =>
	async ({ event, resolve }: any) =>
		path := event.url.pathname
		method := event.request.method
		readable := (method === 'GET' || method === 'HEAD') && !SKIP.test(path)
		if readable
			c := event.locals.container
			s := svc c
			r := await s.redirectFor path
			if r
				await s.hit r.id
				return new Response('Gone', { status: 410, headers: { 'content-type': 'text/plain' } }) if r.status === 410
				location := r.to_path + (r.to_path.includes('?') ? '' : event.url.search)
				return new Response(null, { status: r.status, headers: { location } })
			return text(await s.robots(), 'text/plain') if path === '/robots.txt'
			if path.endsWith('.txt')
				key := await ix(c).keyFile path
				return text(key, 'text/plain') if key
			return text(await s.sitemapIndex(), 'application/xml') if path === '/sitemap.xml'
			rest := event.locals.locale?.rest ?? path
			sm := /^\/sitemap-([a-z0-9_-]+)\.xml$/.exec(rest)
			if sm
				xml := await s.sitemapFor sm[1]
				return text(xml, 'application/xml') if xml
			return text(await s.feed(), 'application/rss+xml') if rest === '/feed.xml'
			return text(await s.llms(), 'text/plain') if rest === '/llms.txt'
			if rest === '/llms-full.txt'
				full := await s.llmsFull()
				return text(full, 'text/plain') if full
			cat := /^\/([^/]+)\/([^/]+)\/([^/]+)\/feed\.xml$/.exec(rest)
			if cat
				lang := event.locals.locale.language
				articleType := cms(c).getType 'article'
				if articleType && `/${cat[1]}` === cms(c).sectionPath(articleType, lang) && cat[2] === i18n(c).word(lang, 'category')
					term := await articles(c).termByLocalSlug 'category', cat[3], lang
					if term
						name := articles(c).localized(term, lang).name
						return text(await s.feed({ termId: term.id, title: name, path: `${event.locals.locale.base}${rest}` }), 'application/rss+xml')
			if rest.endsWith('.md') && rest.length > 3
				target := rest.slice(0, -3)
				md := await s.twin(target === '/index' ? '/' : target)
				if md
					return text(md, 'text/markdown', { 'cache-control': 'public, max-age=300', 'x-robots-tag': 'noindex' })
		response := await resolve event
		if readable && response.status === 404 && !ASSET.test(path)
			try
				ref := event.request.headers.get('referer') ?? ''
				host := ref ? new URL(ref).host : ''
				await svc(event.locals.container).log404 path.slice(0, 300), host
			catch
				// Logging must never break the 404 page.
				response
		response
