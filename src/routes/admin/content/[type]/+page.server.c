import { page } from '@human-synthesis/norns/server'
import { error } from '@sveltejs/kit'
import { cms } from '$lib/norns/content/server/public'
import { requireCap, can } from '$lib/norns/auth/server/public'

PER_PAGE := 50

export load := page.load
	handler: async ({ container, params, url, user }) =>
		requireCap user, 'content.view'
		ct := cms(container).getType params.type
		unless ct
			throw error 404, `Unknown content type "${params.type}"`
		status := url.searchParams.get('status') ?? ''
		q := url.searchParams.get('q') ?? ''
		mine := url.searchParams.get('mine') === '1'
		pageNo := Math.max 1, Number(url.searchParams.get('page')) || 1
		opts := { status: status || undefined, q: q || undefined, authorId: mine ? user.id : undefined }
		entries := await cms(container).list ct.id, { ...opts, limit: PER_PAGE, offset: (pageNo - 1) * PER_PAGE }
		filtered := await cms(container).count ct.id, opts
		tabs := [] as any[]
		for [id, label] of [['', 'All'], ['draft', 'Drafts'], ['in_review', 'In review'], ['scheduled', 'Scheduled'], ['published', 'Live']]
			tabs.push { id, label, count: await cms(container).count(ct.id, { status: id || undefined }) }
		{ ct, entries, status, q, mine, tabs, page: pageNo, pages: Math.max(1, Math.ceil(filtered / PER_PAGE)), canCreate: can(user, 'content.create') }
