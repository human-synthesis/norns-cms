import type { Container } from '@human-synthesis/norns/server'
import { registerEndpoint, apiError, paging, need } from '../../api/server/public'
import { media, MAX_BYTES } from './public'
import { cms } from '../../content/server/public'
import { audit } from '../../auth/server/public'

export registerMediaApi := (app: Container) =>
	registerEndpoint app, {
		method: 'GET', path: '/media', capability: 'media.view', tag: 'Media'
		summary: 'List media, newest first'
		query: [{ name: 'q', description: 'Search file names, alt text, captions' }, { name: 'kind', description: 'image | document' }, { name: 'limit', description: '1–200' }, { name: 'offset', description: 'Default 0' }]
		handler: async ({ c, query }) =>
			{ limit, offset } := paging query, 200
			{ data: await media(c).list({ q: query.q, kind: query.kind, limit, offset }), meta: { total: await media(c).count(), limit, offset } }
	}
	registerEndpoint app, {
		method: 'GET', path: '/media/:id', capability: 'media.view', tag: 'Media'
		summary: 'One media item and where it is used'
		handler: async ({ c, params }) =>
			m := need(await media(c).get(Number(params.id)), 'Media not found')
			{ ...m, usage: await media(c).usage(m.id) }
	}
	registerEndpoint app, {
		method: 'POST', path: '/media', capability: 'media.upload', tag: 'Media', raw: true, created: true
		summary: 'Upload a file: the request body is the file itself'
		description: 'Send the bytes as the body (any Content-Type; the real type is detected). PNG, JPEG, GIF, WebP, AVIF or PDF, up to 10 MB. Query: filename (required), alt, caption.'
		query: [{ name: 'filename', description: 'e.g. photo.jpg' }, { name: 'alt', description: 'Alt text' }, { name: 'caption', description: 'Caption' }]
		body: { description: 'The file bytes' }
		handler: async ({ c, query, event, user }) =>
			throw apiError(400, 'Pass ?filename=') unless query.filename
			bytes := new Uint8Array(await event.request.arrayBuffer())
			throw apiError(413, 'Larger than 10 MB') if bytes.length > MAX_BYTES
			r := await media(c).upload new File([bytes], String(query.filename)), { alt: query.alt ?? '', caption: query.caption ?? '', authorId: user.id }
			throw apiError(400, r.error) unless r.ok
			await audit(c).log user, 'media.upload', r.media.filename, { id: r.media.id, via: 'api' }
			r.media
	}
	registerEndpoint app, {
		method: 'PATCH', path: '/media/:id', capability: 'media.edit', tag: 'Media'
		summary: 'Change alt text or caption (entries using it are re-rendered)'
		body: { description: 'alt, caption', example: { alt: 'A red bicycle against a wall' } }
		handler: async ({ c, params, body, user }) =>
			id := Number(params.id)
			need(await media(c).get(id), 'Media not found')
			await media(c).update id, { alt: body.alt == null ? undefined : String(body.alt).slice(0, 300), caption: body.caption == null ? undefined : String(body.caption).slice(0, 500) }
			used := await media(c).usage id
			await cms(c).rerender used.map((u) => u.id)
			await audit(c).log user, 'media.update', `media#${id}`, { via: 'api' }
			await media(c).get id
	}
	registerEndpoint app, {
		method: 'DELETE', path: '/media/:id', capability: 'media.delete', tag: 'Media'
		summary: 'Delete a media item (refused while used, unless ?force=1)'
		query: [{ name: 'force', description: '1 to delete even when entries use it' }]
		handler: async ({ c, params, query, user }) =>
			id := Number(params.id)
			r := await media(c).remove id, query.force === '1'
			throw apiError((r.usage ? 409 : 404), r.error, { usage: r.usage }) unless r.ok
			await audit(c).log user, 'media.delete', `media#${id}`, { via: 'api' }
			{ deleted: id }
	}
