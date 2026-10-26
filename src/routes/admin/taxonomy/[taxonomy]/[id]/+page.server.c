import { page } from '@human-synthesis/norns/server'
import { error, fail, redirect } from '@sveltejs/kit'
import { articles } from '$lib/norns/articles/server/public'
import { seo } from '$lib/norns/seo/server/public'
import { requireCap, audit } from '$lib/norns/auth/server/public'
import { formToObject } from '$lib/norns/content/server/public'

termOr404 := async (container: any, params: any) =>
	tax := articles(container).taxonomy params.taxonomy
	term := tax ? await articles(container).term(Number(params.id)) : undefined
	unless tax && term && term.taxonomy === tax.id
		throw error 404, 'Not found'
	{ tax, term }

export load := page.load
	handler: async ({ container, params, user }) =>
		requireCap user, 'taxonomy.manage'
		{ tax, term } := await termOr404 container, params
		terms := await articles(container).terms tax.id
		{ tax, term, others: terms.filter((t) => t.id !== term.id), url: articles(container).termUrl(term) }

export actions := page.actions
	save:
		run: async ({ container, event, user }) =>
			requireCap user, 'taxonomy.manage'
			{ tax, term } := await termOr404 container, event.params
			raw := formToObject await event.request.formData()
			r := await articles(container).saveTerm tax.id, term.id, raw
			return fail(400, { errors: [{ path: [{ key: r.field ?? 'name' }], message: r.error }], values: raw }) unless r.ok
			if r.term.slug !== term.slug
				await seo(container).saveRedirect null, { from: articles(container).termUrl(term), to: articles(container).termUrl(r.term), status: 301 }
			await audit(container).log user, 'taxonomy.update', `${tax.id}:${r.term.slug}`, {}
			{ saved: true }
	merge:
		run: async ({ container, event, user }) =>
			requireCap user, 'taxonomy.manage'
			{ tax, term } := await termOr404 container, event.params
			into := Number((await event.request.formData()).get('into'))
			r := await articles(container).mergeTerm term.id, into, user
			return fail(400, { error: r.error }) unless r.ok
			await seo(container).saveRedirect null, { from: articles(container).termUrl(term), to: articles(container).termUrl(r.into), status: 301 }
			await audit(container).log user, 'taxonomy.merge', `${tax.id}:${term.slug}`, { into: r.into.slug }
			throw redirect 303, `/admin/taxonomy/${tax.id}/${r.into.id}`
	remove:
		run: async ({ container, event, user }) =>
			requireCap user, 'taxonomy.manage'
			{ tax, term } := await termOr404 container, event.params
			await articles(container).deleteTerm term.id
			await audit(container).log user, 'taxonomy.delete', `${tax.id}:${term.slug}`, {}
			throw redirect 303, `/admin/taxonomy/${tax.id}`
