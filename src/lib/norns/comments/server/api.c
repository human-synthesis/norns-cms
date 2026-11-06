import type { Container } from '@human-synthesis/norns/server'
import { registerEndpoint, apiError, need } from '../../api/server/public'
import { audit } from '../../auth/server/public'
import { comments } from './public'
import { STATUSES } from '../shared/settings'

export registerCommentsApi := (app: Container) =>
	registerEndpoint app, {
		method: 'GET', path: '/comments', capability: 'comments.moderate', tag: 'Comments'
		summary: 'Comments, newest first, with their entry and the count per status'
		query: [
			{ name: 'status', description: 'pending | approved | spam | trash' }
			{ name: 'entry', description: 'Entry id' }
			{ name: 'q', description: 'Search text, name and email' }
			{ name: 'page', description: 'Page (50 per page, perPage up to 200)' }
		]
		handler: ({ c, query }) => comments(c).list { status: query.status, entryId: query.entry, q: query.q, page: query.page, perPage: query.perPage }
	}
	registerEndpoint app, {
		method: 'GET', path: '/comments/:id', capability: 'comments.moderate', tag: 'Comments'
		summary: 'One comment'
		handler: async ({ c, params }) => need(await comments(c).get(Number(params.id)), 'Comment not found')
	}
	registerEndpoint app, {
		method: 'POST', path: '/comments', capability: 'comments.moderate', tag: 'Comments', created: true
		summary: "Comment on an entry as the key's owner (the team): approved at once; parentId answers a comment"
		body: { description: 'entryId, body, parentId?', example: { entryId: 12, body: 'Thanks for the question!' } }
		handler: async ({ c, body, user }) =>
			r := await comments(c).post { entryId: body.entryId, parentId: body.parentId, body: body.body }, { user, viaApi: true }
			throw apiError((r.status ?? 400), r.error ?? 'Invalid input', { errors: r.errors }) unless r.ok
			await audit(c).log user, 'comments.post', `comment#${r.id}`, { via: 'api' }
			comments(c).get r.id
	}
	registerEndpoint app, {
		method: 'POST', path: '/comments/:id/reply', capability: 'comments.moderate', tag: 'Comments', created: true
		summary: 'Answer a comment as the team (approves it too when it was waiting)'
		body: { description: 'body', example: { body: 'Good point — fixed.' } }
		handler: async ({ c, params, body, user }) =>
			r := await comments(c).reply Number(params.id), String(body.body ?? ''), user
			throw apiError((r.status ?? 400), r.error ?? 'Invalid input') unless r.ok
			await audit(c).log user, 'comments.reply', `comment#${params.id}`, { via: 'api' }
			comments(c).get r.id
	}
	registerEndpoint app, {
		method: 'PATCH', path: '/comments/:id', capability: 'comments.moderate', tag: 'Comments'
		summary: 'Approve, mark as spam, move to trash, or correct the text'
		body: { description: 'status (pending | approved | spam | trash), body', example: { status: 'approved' } }
		handler: async ({ c, params, body, user }) =>
			id := Number params.id
			need (await comments(c).get(id)), 'Comment not found'
			if body.status !== undefined
				throw apiError(400, `status must be one of ${STATUSES.join(', ')}`) unless STATUSES.includes(body.status)
				await comments(c).setStatus id, body.status, user
			if body.body !== undefined
				throw apiError(400, 'body cannot be empty') unless await comments(c).edit(id, String(body.body))
			await audit(c).log user, 'comments.moderate', `comment#${id}`, { via: 'api', status: body.status, edited: body.body !== undefined }
			comments(c).get id
	}
	registerEndpoint app, {
		method: 'DELETE', path: '/comments/:id', capability: 'comments.moderate', tag: 'Comments'
		summary: 'Delete a comment for good (its replies move up a level)'
		handler: async ({ c, params, user }) =>
			id := Number params.id
			need (await comments(c).get(id)), 'Comment not found'
			await comments(c).remove id
			await audit(c).log user, 'comments.delete', `comment#${id}`, { via: 'api' }
			{ deleted: id }
	}
