import type { Container } from '@human-synthesis/norns/server'
import { registerEndpoint, apiError, validationError, paging, need, allow } from '../../api/server/public'
import { cms, canEditEntry, canRemoveEntry, renderMarkdown, collectRefs } from './public'
import { can, audit } from '../../auth/server/public'

// Entries as API objects: data plus URL, without the cached render tree.
present := (c: Container, ct: any, e: any) =>
	{ rendered, ...rest } := e
	{ ...rest, url: cms(c).urlFor(ct, e) }

typeOf := (c: Container, id: string) =>
	need cms(c).getType(id), `Unknown content type "${id}"`

entryOf := async (c: Container, ct: any, id: string) =>
	entry := await cms(c).get(ct.id, Number(id))
	need entry, 'Entry not found'

parseAt := (v: any) =>
	return null if v == null || v === ''
	ms := typeof v === 'number' ? v : Date.parse(String(v))
	throw apiError(400, 'publishAt must be an ISO date or a timestamp in ms') unless Number.isFinite(ms)
	ms

// Status changes with the same rules as the editor: publishing needs
// content.publish; submitting for review needs edit rights.
setStatus := async (c: Container, ct: any, entry: any, status: string, publishAt: any, user: any) =>
	if status === 'published' || (entry.status === 'published' && status !== 'published')
		allow can(user, 'content.publish'), 'Publishing needs the content.publish capability'
	else
		allow canEditEntry(user, entry), 'This key cannot edit this entry'
	throw apiError(400, 'status must be draft, in_review or published') unless ['draft', 'in_review', 'published'].includes(status)
	await cms(c).setStatus ct.id, entry.id, status, { publishAt: parseAt(publishAt), user }
	await audit(c).log user, `content.${status === 'published' ? 'publish' : status === 'in_review' ? 'submit' : 'unpublish'}`, `${ct.id}#${entry.id}`, { via: 'api' }
	await cms(c).get ct.id, entry.id

export registerContentApi := (app: Container) =>
	registerEndpoint app, {
		method: 'GET', path: '/types', capability: 'content.view', tag: 'Content'
		summary: 'Content types with their fields, tabs and the available blocks'
		handler: async ({ c }) =>
			out := [] as any[]
			for ct of cms(c).types()
				form := await cms(c).form ct.id
				out.push { id: ct.id, label: ct.label, labelPlural: ct.labelPlural, basePath: ct.basePath, listable: !!ct.listable, aliases: ct.aliases ?? [], tabs: form.tabs, fields: form.fields }
			{ data: out, meta: { blocks: (await cms(c).form(out[0]?.id ?? 'page')).blocks } }
	}
	registerEndpoint app, {
		method: 'GET', path: '/content/:type', capability: 'content.view', tag: 'Content'
		summary: 'List entries of a type'
		query: [
			{ name: 'status', description: 'draft | in_review | scheduled | published (live)' }
			{ name: 'q', description: 'Search titles and slugs' }
			{ name: 'author', description: 'A user id, or "me"' }
			{ name: 'limit', description: '1–100, default 50' }
			{ name: 'offset', description: 'Default 0' }
		]
		handler: async ({ c, params, query, user }) =>
			ct := typeOf c, params.type
			{ limit, offset } := paging query
			author := query.author === 'me' ? user.id : (Number(query.author) || undefined)
			opts := { status: query.status || undefined, q: query.q || undefined, authorId: author }
			entries := await cms(c).list ct.id, { ...opts, limit, offset }
			{ data: entries.map((e) => present(c, ct, e)), meta: { total: await cms(c).count(ct.id, opts), limit, offset } }
	}
	registerEndpoint app, {
		method: 'POST', path: '/content/:type', capability: 'content.create', tag: 'Content', created: true
		summary: 'Create an entry (a draft unless status says otherwise)'
		body: { description: 'data: field values by field name (see GET /types). Optional slug, visibility (public | members), status, publishAt (ISO).', example: { data: { title: 'Hello', summary: 'One line.', body: '## Hi\n\nText', tags: ['news'] }, visibility: 'public', status: 'draft' } }
		handler: async ({ c, params, body, user }) =>
			ct := typeOf c, params.type
			r := await cms(c).save ct.id, null, { ...(body.data ?? {}), slug: body.slug ?? '', visibility: body.visibility ?? 'public' }, user
			throw validationError(r.issues) unless r.ok
			await audit(c).log user, 'content.create', `${ct.id}#${r.id}`, { via: 'api', title: r.entry.title }
			entry .= r.entry
			if body.status && body.status !== 'draft'
				entry = await setStatus c, ct, entry, body.status, body.publishAt, user
			present c, ct, entry
	}
	registerEndpoint app, {
		method: 'GET', path: '/content/:type/:id', capability: 'content.view', tag: 'Content'
		summary: 'One entry, with its rendered HTML per Markdown field'
		handler: async ({ c, params }) =>
			ct := typeOf c, params.type
			e := await entryOf c, ct, params.id
			{ ...present(c, ct, e), html: cms(c).htmlOf(e) }
	}
	registerEndpoint app, {
		method: 'PATCH', path: '/content/:type/:id', capability: 'content.view', tag: 'Content'
		summary: 'Update an entry: fields in data are merged into the current values'
		description: 'Needs content.edit_any, or content.edit_own for your own unpublished entries. Saving a published entry changes the live site.'
		body: { description: 'data (partial field values), slug, visibility', example: { data: { summary: 'Better summary.' } } }
		handler: async ({ c, params, body, user }) =>
			ct := typeOf c, params.type
			e := await entryOf c, ct, params.id
			allow canEditEntry(user, e), 'This key cannot edit this entry'
			raw := { ...e.data, ...(body.data ?? {}), slug: body.slug ?? e.slug, visibility: body.visibility ?? e.visibility }
			r := await cms(c).save ct.id, e.id, raw, user
			throw validationError(r.issues) unless r.ok
			await audit(c).log user, 'content.update', `${ct.id}#${e.id}`, { via: 'api', fields: Object.keys(body.data ?? {}) }
			present c, ct, r.entry
	}
	registerEndpoint app, {
		method: 'DELETE', path: '/content/:type/:id', capability: 'content.view', tag: 'Content'
		summary: 'Delete an entry'
		description: 'Needs content.delete, or content.edit_own for your own unpublished entries.'
		handler: async ({ c, params, user }) =>
			ct := typeOf c, params.type
			e := await entryOf c, ct, params.id
			allow canRemoveEntry(user, e), 'This key cannot delete this entry'
			await cms(c).remove ct.id, e.id, user
			await audit(c).log user, 'content.delete', `${ct.id}#${e.id}`, { via: 'api', title: e.title }
			{ deleted: e.id }
	}
	registerEndpoint app, {
		method: 'POST', path: '/content/:type/:id/status', capability: 'content.view', tag: 'Content'
		summary: 'Change status: draft, in_review, or published (now or scheduled with publishAt)'
		body: { description: 'status, publishAt (ISO; future = scheduled)', example: { status: 'published', publishAt: '2026-11-01T09:00:00Z' } }
		handler: async ({ c, params, body, user }) =>
			ct := typeOf c, params.type
			e := await entryOf c, ct, params.id
			present c, ct, await setStatus(c, ct, e, String(body.status ?? ''), body.publishAt, user)
	}
	registerEndpoint app, {
		method: 'GET', path: '/content/:type/:id/revisions', capability: 'content.view', tag: 'Content'
		summary: 'Revisions of an entry, newest first (with their data)'
		handler: async ({ c, params }) =>
			ct := typeOf c, params.type
			e := await entryOf c, ct, params.id
			(await cms(c).revisions e.id).map((r) => ({ id: r.id, title: r.title, slug: r.slug, created_at: r.created_at, author: r.author_email ?? null, data: r.data }))
	}
	registerEndpoint app, {
		method: 'POST', path: '/content/:type/:id/revisions/:rev/restore', capability: 'content.view', tag: 'Content'
		summary: 'Restore a revision (saved as a new revision)'
		handler: async ({ c, params, user }) =>
			ct := typeOf c, params.type
			e := await entryOf c, ct, params.id
			allow canEditEntry(user, e), 'This key cannot edit this entry'
			r := await cms(c).restore ct.id, e.id, Number(params.rev), user
			throw validationError(r.issues) unless r.ok
			await audit(c).log user, 'content.restore', `${ct.id}#${e.id}`, { via: 'api', revision: Number(params.rev) }
			present c, ct, r.entry
	}
	registerEndpoint app, {
		method: 'POST', path: '/render', capability: 'content.view', tag: 'Content'
		summary: 'Render Markdown with the site renderer (media resolved, sanitised)'
		body: { description: 'markdown', example: { markdown: '## Title\n\n:::tip\nHello\n:::' } }
		handler: async ({ c, body }) =>
			md := String(body.markdown ?? '').slice(0, 500000)
			renderMarkdown md, { media: await cms(c).lookupMedia(collectRefs(md).media) }
	}
