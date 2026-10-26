import { page } from '@human-synthesis/norns/server'
import { error, redirect, fail } from '@sveltejs/kit'
import { cms, canEditEntry } from '$lib/norns/content/server/public'
import { requireCap, audit } from '$lib/norns/auth/server/public'

load_ := async (container: any, params: any) =>
	ct := cms(container).getType params.type
	unless ct
		throw error 404, 'Unknown content type'
	entry := await cms(container).get ct.id, Number(params.id)
	unless entry
		throw error 404, 'Entry not found'
	{ ct, entry }

// Field-by-field differences between two data objects, as display strings.
diff := (fields: any[], a: any, b: any) =>
	show := (x: any) => typeof x === 'string' ? x : JSON.stringify(x ?? '', null, 1)
	out := [] as any[]
	for f of fields
		left := show a?.[f.name]
		right := show b?.[f.name]
		out.push { name: f.name, label: f.label, before: left, after: right } if left !== right
	out

export load := page.load
	handler: async ({ container, params, url, user }) =>
		requireCap user, 'content.view'
		{ ct, entry } := await load_ container, params
		revisions := await cms(container).revisions entry.id
		selected := Number(url.searchParams.get('rev')) || null
		rev := selected ? revisions.find((r) => r.id === selected) : null
		fields := cms(container).fields ct
		{
			ct
			entry
			revisions: revisions.map((r) => ({ id: r.id, title: r.title, created_at: r.created_at, author: r.author_name || r.author_email || '—' }))
			selected: rev ? { id: rev.id, title: rev.title, slug: rev.slug, created_at: rev.created_at, changes: diff(fields, rev.data, entry.data) } : null
			canRestore: canEditEntry(user, entry)
		}

export actions := page.actions
	restore:
		run: async ({ container, event, user }) =>
			{ ct, entry } := await load_ container, event.params
			unless canEditEntry(user, entry)
				throw error 403, 'You cannot edit this entry'
			revId := Number((await event.request.formData()).get('rev'))
			result := await cms(container).restore ct.id, entry.id, revId, user
			return fail(400, { error: result.issues?.[0]?.message ?? 'That revision no longer validates' }) unless result.ok
			await audit(container).log user, 'content.restore', `${ct.id}#${entry.id}`, { revision: revId }
			throw redirect 303, `/admin/content/${ct.id}/${entry.id}?saved=saved`
