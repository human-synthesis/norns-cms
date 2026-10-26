import { page } from '@human-synthesis/norns/server'
import { error } from '@sveltejs/kit'
import { articles } from '$lib/norns/articles/server/public'
import { seo } from '$lib/norns/seo/server/public'
import { can } from '$lib/norns/auth/server/public'

export load := page.load
	handler: async ({ container, params, url, user }) =>
		term := await articles(container).termBySlug 'tag', params.slug
		unless term
			throw error 404, 'Not found'
		pageNo := Math.max 1, Number(url.searchParams.get('page')) || 1
		list := await articles(container).list { page: pageNo, perPage: 12, termId: term.id, member: can(user, 'content.members') }
		path := articles(container).termUrl term
		{
			term
			label: 'Tag'
			path
			list
			seo: await seo(container).forPage({ title: term.name, description: term.description || undefined, path: pageNo > 1 ? `${path}?page=${pageNo}` : path, noindex: list.total === 0, crumbs: [{ name: 'Articles', url: '/articles' }, { name: term.name, url: path }] })
		}
