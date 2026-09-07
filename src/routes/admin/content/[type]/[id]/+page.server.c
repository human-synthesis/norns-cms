import { page } from '@human-synthesis/norns/server'
import { error, fail, redirect } from '@sveltejs/kit'
import { cms } from '$lib/norns/content/server/public'

typeOr404 := (container, id) =>
	ct := cms(container).getType id
	unless ct
		throw error 404, `Unknown content type "${id}"`
	ct

export load := page.load
	handler: async ({ container, params, url }) =>
		ct := typeOr404 container, params.type
		isNew := params.id === 'new'
		entry := isNew ? null : await cms(container).get(ct.id, Number(params.id))
		if !isNew && !entry
			throw error 404, 'Entry not found'
		values := entry ? { ...entry.data, slug: entry.slug } : {}
		{
			ct,
			entry,
			form: cms(container).form(ct.id),
			values,
			saved: url.searchParams.has('saved'),
			publicUrl: entry ? cms(container).urlFor(ct, entry) : null,
		}

export actions := page.actions
	// Actions receive { input, container, event, user }; route params come
	// from event.params.
	save:
		run: async ({ container, event, user }) =>
			{ params } := event
			ct := typeOr404 container, params.type
			raw := Object.fromEntries(await event.request.formData()) as Record<string, any>
			id := params.id === 'new' ? null : Number(params.id)
			result := await cms(container).save ct.id, id, raw, user?.id ?? null
			unless result.ok
				return fail 400, { errors: result.issues, values: raw }
			throw redirect 303, `/admin/content/${ct.id}/${result.id}?saved=1`
	publish:
		run: async ({ container, event }) =>
			{ params } := event
			ct := typeOr404 container, params.type
			id := Number(params.id)
			entry := await cms(container).get ct.id, id
			unless entry
				throw error 404, 'Entry not found'
			await cms(container).setStatus ct.id, id, entry.status === 'published' ? 'draft' : 'published'
			throw redirect 303, `/admin/content/${ct.id}/${id}?saved=1`
	remove:
		run: async ({ container, event }) =>
			{ params } := event
			ct := typeOr404 container, params.type
			await cms(container).remove ct.id, Number(params.id)
			throw redirect 303, `/admin/content/${ct.id}`
