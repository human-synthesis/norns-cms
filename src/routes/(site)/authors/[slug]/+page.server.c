import { page } from '@human-synthesis/norns/server'
import { error } from '@sveltejs/kit'
import { articles } from '$lib/norns/articles/server/public'
import { seo } from '$lib/norns/seo/server/public'
import { cms } from '$lib/norns/content/server/public'
import { auth, can } from '$lib/norns/auth/server/public'

export load := page.load
	handler: async ({ container, params, url, user }) =>
		author := await auth(container).authorBySlug params.slug
		unless author
			throw error 404, 'Not found'
		pageNo := Math.max 1, Number(url.searchParams.get('page')) || 1
		list := await articles(container).byAuthor author.id, { page: pageNo, member: can(user, 'content.members') }
		avatar := author.avatar ? (await cms(container).lookupMedia([author.avatar]))[author.avatar] : null
		path := `/authors/${author.slug}`
		head := await seo(container).forPage { title: author.name, description: author.bio || `Articles by ${author.name}`, path, noindex: list.total === 0, crumbs: [{ name: author.name, url: path }] }
		{ author, avatar, list, path, seo: head }
