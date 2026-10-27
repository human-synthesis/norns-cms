import { route } from '@human-synthesis/norns/server'
import { error } from '@sveltejs/kit'
import { forms } from '$lib/norns/forms/server/public'
import { requireCap, audit } from '$lib/norns/auth/server/public'

export GET := route
	handler: async ({ container, event, user }) =>
		requireCap user, 'forms.view'
		r := await forms(container).csv Number(event.params.id)
		unless r
			throw error 404, 'Form not found'
		await audit(container).log user, 'forms.export', r.name, {}
		new Response(r.csv, { headers: { 'content-type': 'text/csv; charset=utf-8', 'content-disposition': `attachment; filename="${r.name}-${new Date().toISOString().slice(0, 10)}.csv"` } })
