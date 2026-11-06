import { page } from '@human-synthesis/norns/server'
import { fail } from '@sveltejs/kit'
import { comments } from '$lib/norns/comments/server/public'
import { requireCap, audit } from '$lib/norns/auth/server/public'

TABS := [
	{ id: 'pending', label: 'Waiting' }
	{ id: 'approved', label: 'Approved' }
	{ id: 'spam', label: 'Spam' }
	{ id: 'trash', label: 'Trash' }
]

export load := page.load
	handler: async ({ container, url, user }) =>
		requireCap user, 'comments.moderate'
		status := url.searchParams.get('status') || 'pending'
		q := url.searchParams.get('q') ?? ''
		pageNo := Math.max 1, Number(url.searchParams.get('page')) || 1
		r := await comments(container).list { status, q, page: pageNo, perPage: 30 }
		config := await comments(container).config()
		{
			...r
			status, q
			pages: Math.max(1, Math.ceil(r.total / r.perPage))
			tabs: TABS.map((t) => ({ ...t, count: r.counts[t.id] ?? 0 }))
			enabled: config.enabled
			types: config.types
		}

idOf := async (event: any) =>
	fd := await event.request.formData()
	{ id: Number(fd.get('id')), fd }

moderate := (status: string, message: string) =>
	run: async ({ container, event, user }) =>
		requireCap user, 'comments.moderate'
		{ id } := await idOf event
		return fail(404, { error: 'Comment not found' }) unless await comments(container).setStatus(id, status, user)
		await audit(container).log user, 'comments.moderate', `comment#${id}`, { status }
		{ saved: message }

export actions := page.actions
	approve: moderate 'approved', 'Comment approved.'
	unapprove: moderate 'pending', 'Comment moved back to Waiting.'
	spam: moderate 'spam', 'Marked as spam.'
	trash: moderate 'trash', 'Moved to Trash.'
	restore: moderate 'pending', 'Restored to Waiting.'
	remove:
		run: async ({ container, event, user }) =>
			requireCap user, 'comments.moderate'
			{ id } := await idOf event
			await comments(container).remove id
			await audit(container).log user, 'comments.delete', `comment#${id}`, {}
			{ saved: 'Comment deleted.' }
	edit:
		run: async ({ container, event, user }) =>
			requireCap user, 'comments.moderate'
			{ id, fd } := await idOf event
			return fail(400, { error: 'The text cannot be empty.' }) unless await comments(container).edit(id, String(fd.get('body') ?? ''))
			await audit(container).log user, 'comments.moderate', `comment#${id}`, { edited: true }
			{ saved: 'Comment updated.' }
	reply:
		run: async ({ container, event, user }) =>
			requireCap user, 'comments.moderate'
			{ id, fd } := await idOf event
			r := await comments(container).reply id, String(fd.get('body') ?? ''), user
			return fail((r.status ?? 400), { error: r.error }) unless r.ok
			await audit(container).log user, 'comments.reply', `comment#${id}`, {}
			{ saved: 'Your answer is published.' }
