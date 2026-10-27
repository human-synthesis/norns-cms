import { page } from '@human-synthesis/norns/server'
import { error } from '@sveltejs/kit'
import { cms, typeOrRedirect } from '$lib/norns/content/server/public'
import { requireCap, can } from '$lib/norns/auth/server/public'

PER_PAGE := 50

export load := page.load
	handler: async ({ container, params, url, user }) =>
		requireCap user, 'content.view'
		ct := typeOrRedirect container, params.type, url
		status := url.searchParams.get('status') ?? ''
		q := url.searchParams.get('q') ?? ''
		mine := url.searchParams.get('mine') === '1'
		lang := url.searchParams.get('lang') ?? ''
		pageNo := Math.max 1, Number(url.searchParams.get('page')) || 1
		opts := { status: status || undefined, q: q || undefined, authorId: mine ? user.id : undefined, locale: lang || undefined }
		entries := await cms(container).list ct.id, { ...opts, limit: PER_PAGE, offset: (pageNo - 1) * PER_PAGE }
		filtered := await cms(container).count ct.id, opts
		tabs := [] as any[]
		for [id, label] of [['', 'All'], ['draft', 'Drafts'], ['in_review', 'In review'], ['scheduled', 'Scheduled'], ['published', 'Live']]
			tabs.push { id, label, count: await cms(container).count(ct.id, { status: id || undefined }) }
		languages := cms(container).languages()
		coverage := languages.length > 1 ? await cms(container).coverage(entries) : {}
		{ ct, entries, status, q, mine, lang, languages, coverage, tabs, page: pageNo, pages: Math.max(1, Math.ceil(filtered / PER_PAGE)), canCreate: can(user, 'content.create') }
