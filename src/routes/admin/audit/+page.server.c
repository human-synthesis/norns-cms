import { page } from '@human-synthesis/norns/server'
import { audit, requireCap } from '$lib/norns/auth/server/public'

PER := 100

export load := page.load
	handler: async ({ container, url, user }) =>
		requireCap user, 'audit.view'
		q := url.searchParams.get('q') ?? ''
		action := url.searchParams.get('action') ?? ''
		pageNo := Math.max 1, Number(url.searchParams.get('page')) || 1
		rows := await audit(container).list { q: q || undefined, action: action || undefined, limit: PER + 1, offset: (pageNo - 1) * PER }
		{ rows: rows.slice(0, PER), more: rows.length > PER, q, action, page: pageNo }
