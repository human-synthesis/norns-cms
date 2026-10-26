import { page } from '@human-synthesis/norns/server'
import { articles } from '$lib/norns/articles/server/public'
import { seo } from '$lib/norns/seo/server/public'
import { can } from '$lib/norns/auth/server/public'

export load := page.load
	handler: async ({ container, url, user }) =>
		pageNo := Math.max 1, Number(url.searchParams.get('page')) || 1
		member := can user, 'content.members'
		featured := pageNo === 1 ? (await articles(container).list({ perPage: 3, featured: true, member })).entries : []
		list := await articles(container).list { page: pageNo, perPage: 12, member }
		rest := list.entries.filter((e) => !featured.some((f) => f.id === e.id))
		categories := (await articles(container).terms('category')).filter((t) => t.count > 0).map((t) => ({ ...t, url: articles(container).termUrl(t) }))
		{
			featured
			entries: rest
			page: list.page
			pages: list.pages
			categories
			seo: await seo(container).forPage({ title: pageNo > 1 ? `Articles — page ${pageNo}` : 'Articles', path: pageNo > 1 ? `/articles?page=${pageNo}` : '/articles', crumbs: [{ name: 'Articles', url: '/articles' }] })
		}
