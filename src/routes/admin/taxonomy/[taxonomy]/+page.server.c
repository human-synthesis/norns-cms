import { page } from '@human-synthesis/norns/server'
import { error, fail } from '@sveltejs/kit'
import { articles } from '$lib/norns/articles/server/public'
import { requireCap, audit } from '$lib/norns/auth/server/public'
import { formToObject } from '$lib/norns/content/server/public'

taxOr404 := (container: any, id: string) =>
	tax := articles(container).taxonomy id
	unless tax
		throw error 404, 'Unknown taxonomy'
	tax

export load := page.load
	handler: async ({ container, params, user }) =>
		requireCap user, 'taxonomy.manage'
		tax := taxOr404 container, params.taxonomy
		{ tax, terms: await articles(container).terms(tax.id) }

export actions := page.actions
	create:
		run: async ({ container, event, user }) =>
			requireCap user, 'taxonomy.manage'
			tax := taxOr404 container, event.params.taxonomy
			raw := formToObject await event.request.formData()
			r := await articles(container).saveTerm tax.id, null, raw
			return fail(400, { errors: [{ path: [{ key: r.field ?? 'name' }], message: r.error }], values: raw }) unless r.ok
			await audit(container).log user, 'taxonomy.create', `${tax.id}:${r.term.slug}`, {}
			{ created: r.term.name }
