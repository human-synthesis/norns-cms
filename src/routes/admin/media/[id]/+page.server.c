import { page } from '@human-synthesis/norns/server'
import { error, fail, redirect } from '@sveltejs/kit'
import { media } from '$lib/norns/media/server/public'
import { cms } from '$lib/norns/content/server/public'
import { requireCap, can, audit } from '$lib/norns/auth/server/public'

export load := page.load
	handler: async ({ container, params, user }) =>
		requireCap user, 'media.view'
		m := await media(container).get Number(params.id)
		unless m
			throw error 404, 'Not found'
		{ m, usage: await media(container).usage(m.id), canEdit: can(user, 'media.edit'), canDelete: can(user, 'media.delete') }

export actions := page.actions
	save:
		run: async ({ container, event, user }) =>
			requireCap user, 'media.edit'
			id := Number(event.params.id)
			fd := await event.request.formData()
			await media(container).update id, { alt: String(fd.get('alt') ?? '').slice(0, 300), caption: String(fd.get('caption') ?? '').slice(0, 500) }
			// Cached HTML embeds alt text and captions: refresh the entries
			// that use this item.
			used := await media(container).usage id
			await cms(container).rerender used.map((u) => u.id)
			await audit(container).log user, 'media.update', `media#${id}`, {}
			{ saved: true, rerendered: used.length }
	remove:
		run: async ({ container, event, user }) =>
			requireCap user, 'media.delete'
			id := Number(event.params.id)
			force := (await event.request.formData()).get('force') === '1'
			r := await media(container).remove id, force
			return fail(400, { error: r.error }) unless r.ok
			await audit(container).log user, 'media.delete', `media#${id}`, { force }
			throw redirect 303, '/admin/media'
