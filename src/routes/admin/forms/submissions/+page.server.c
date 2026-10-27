import { page } from '@human-synthesis/norns/server'
import { forms } from '$lib/norns/forms/server/public'
import { requireCap, can } from '$lib/norns/auth/server/public'

PER := 50

export load := page.load
	handler: async ({ container, url, user }) =>
		requireCap user, 'forms.view'
		formId := Number(url.searchParams.get('form')) || undefined
		status := url.searchParams.get('status') ?? ''
		q := url.searchParams.get('q') ?? ''
		pageNo := Math.max 1, Number(url.searchParams.get('page')) || 1
		filter := { formId, status: status || undefined, q: q || undefined }
		rows := await forms(container).submissions { ...filter, limit: PER, offset: (pageNo - 1) * PER }
		tabs := [] as any[]
		for [id, label] of [['', 'Inbox'], ['new', 'New'], ['read', 'Read'], ['archived', 'Archived'], ['spam', 'Spam']]
			tabs.push { id, label, count: await forms(container).count({ formId, status: id || undefined }) }
		{ rows, forms: await forms(container).list(), formId, status, q, tabs, page: pageNo, pages: Math.max(1, Math.ceil((await forms(container).count(filter)) / PER)), canManage: can(user, 'forms.manage') }
