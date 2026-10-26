import { route } from '@human-synthesis/norns/server'
import { error } from '@sveltejs/kit'
import { media } from '$lib/norns/media/server/public'

// Media files from R2 (or the local media directory). Keys are random and
// immutable, so responses cache for a year. Only sniffed, allow-listed
// types are ever stored (no SVG / HTML), and nosniff stops reinterpretation.
export GET := route
	handler: async ({ container, event }) =>
		obj := await media(container).read event.params.key
		unless obj
			throw error 404, 'Not found'
		headers := {
			'content-type': obj.mime
			'cache-control': 'public, max-age=31536000, immutable'
			'x-content-type-options': 'nosniff'
			'content-security-policy': "default-src 'none'; sandbox"
		} as Record<string, string>
		headers['content-disposition'] = `inline; filename="${obj.filename.replace(/"/g, '')}"` if obj.mime === 'application/pdf'
		headers.etag = obj.etag if obj.etag
		new Response(obj.body, { headers })
