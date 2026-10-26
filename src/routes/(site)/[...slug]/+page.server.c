import { page } from '@human-synthesis/norns/server'
import { error } from '@sveltejs/kit'
import { cms } from '$lib/norns/content/server/public'
import { articles } from '$lib/norns/articles/server/public'
import { seo } from '$lib/norns/seo/server/public'
import { consent } from '$lib/norns/consent/server/public'
import { can, isStaff } from '$lib/norns/auth/server/public'

// Any entry by URL. Staff see drafts and scheduled entries (preview);
// members read members-only entries; everyone else gets them locked.
export load := page.load
	handler: async ({ container, params, user }) =>
		viewer := { preview: isStaff(user), member: can(user, 'content.members') }
		res := await cms(container).resolve params.slug, viewer
		unless res
			// A built-in cookie declaration until a page takes the /cookies URL.
			if params.slug === 'cookies'
				return { kind: 'cookies', declaration: await consent(container).declaration(), seo: await seo(container).forPage({ title: 'Cookies', path: '/cookies' }) }
			throw error 404, 'Page not found'
		if res.kind === 'list'
			return { kind: 'list', ct: res.ct, entries: res.entries, seo: await seo(container).forPage({ title: res.ct.labelPlural, path: res.ct.basePath }) }
		extra := { authors: [], category: null, tags: [], sources: [], related: [], avatars: {} } as Record<string, any>
		if res.ct.id === 'article'
			[decorated] := await articles(container).decorate [res.entry]
			extra.authors = decorated.authors
			extra.category = decorated.category
			extra.tags = decorated.tagTerms
			extra.sources = res.locked ? [] : articles(container).sources(res.entry)
			extra.related = await articles(container).related res.entry
			extra.avatars = await cms(container).lookupMedia extra.authors.map((p) => p.avatar).filter(Boolean)
		{ kind: 'entry', res, extra, seo: await seo(container).forEntry(res, extra) }
