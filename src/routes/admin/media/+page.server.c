import { page } from '@human-synthesis/norns/server'
import { fail } from '@sveltejs/kit'
import { media } from '$lib/norns/media/server/public'
import { requireCap, can, audit } from '$lib/norns/auth/server/public'

export load := page.load
	handler: async ({ container, url, user }) =>
		requireCap user, 'media.view'
		q := url.searchParams.get('q') ?? ''
		kind := url.searchParams.get('kind') ?? ''
		{ items: await media(container).list({ q, kind, limit: 200 }), q, kind, canUpload: can(user, 'media.upload'), total: await media(container).count() }

export actions := page.actions
	upload:
		run: async ({ container, event, user }) =>
			requireCap user, 'media.upload'
			fd := await event.request.formData()
			files := fd.getAll('files').filter((f) => typeof f === 'object' && (f as File).size > 0) as File[]
			return fail(400, { error: 'Choose one or more files' }) unless files.length
			errors := [] as string[]
			n .= 0
			for file of files
				r := await media(container).upload file, { authorId: user.id }
				if r.ok
					n++
					await audit(container).log user, 'media.upload', r.media.filename, { id: r.media.id }
				else
					errors.push r.error
			{ uploaded: n, errors }
