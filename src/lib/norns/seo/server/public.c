import type { Container } from '@human-synthesis/norns/server'
import type { SeoService } from './service'

export { seoChecks, AI_BOTS } from '../shared/fields'

svc := (c: Container) => c.resolve('seo.service') as SeoService

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
	location: () => svc(c).location()
	redirects: (q?: string) => svc(c).redirects(q)
	saveRedirect: (id: number | null, input: any) => svc(c).saveRedirect(id, input)
	removeRedirect: (id: number) => svc(c).removeRedirect(id)
	importRedirects: (text: string) => svc(c).importRedirects(text)
	notFound: () => svc(c).notFound()
	clearNotFound: (path: string | null) => svc(c).clearNotFound(path)
	count404: () => svc(c).count404()
	report: () => svc(c).report()
	purge: () => svc(c).purge()
}

SKIP := /^\/(admin|api|account|login|media|_app|cdn-cgi|reset|invite|verify)(\/|$)/
ASSET := /\.(js|css|map|png|jpe?g|gif|webp|avif|svg|ico|woff2?|ttf|txt|xml|json)$/i

// Redirects (before routing), Markdown twins (`/<path>.md`) and the 404 log.
// Runs after the session handle, so it can skip the work for admin traffic.
export seoHandle := =>
	async ({ event, resolve }: any) =>
		path := event.url.pathname
		method := event.request.method
		readable := (method === 'GET' || method === 'HEAD') && !SKIP.test(path)
		if readable
			c := event.locals.container
			r := await svc(c).redirectFor path
			if r
				await svc(c).hit r.id
				return new Response('Gone', { status: 410, headers: { 'content-type': 'text/plain' } }) if r.status === 410
				location := r.to_path + (r.to_path.includes('?') ? '' : event.url.search)
				return new Response(null, { status: r.status, headers: { location } })
			if path.endsWith('.md') && path.length > 3
				target := path.slice(0, -3)
				md := await svc(c).twin(target === '/index' ? '/' : target)
				if md
					return new Response(md, { headers: { 'content-type': 'text/markdown; charset=utf-8', 'cache-control': 'public, max-age=300', 'x-robots-tag': 'noindex' } })
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
