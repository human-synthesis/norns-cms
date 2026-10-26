import { route } from '@human-synthesis/norns/server'
import { json, error } from '@sveltejs/kit'
import { media } from '$lib/norns/media/server/public'
import { requireCap, audit } from '$lib/norns/auth/server/public'

// GET ?q=&kind=image → { items } for the media dialog.
export GET := route
	handler: async ({ container, event, user }) =>
		requireCap user, 'media.view'
		q := event.url.searchParams.get('q') ?? ''
		kind := event.url.searchParams.get('kind') ?? ''
		json { items: await media(container).list({ q, kind, limit: 120 }) }

// POST multipart (file, alt?) → the media item. Used by the dialog and by
// paste / drop in the Markdown editor.
export POST := route
	handler: async ({ container, event, user }) =>
		requireCap user, 'media.upload'
		fd := await event.request.formData()
		file := fd.get 'file'
		unless file && typeof file === 'object'
			throw error 400, 'Choose a file'
		r := await media(container).upload file as File, { alt: String(fd.get('alt') ?? ''), authorId: user.id }
		unless r.ok
			throw error 400, r.error
		await audit(container).log user, 'media.upload', r.media.filename, { id: r.media.id }
		json r.media
