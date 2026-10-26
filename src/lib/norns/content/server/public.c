import type { Container } from '@human-synthesis/norns/server'
import type { ContentService, Viewer } from './service'
import type { ContentType, BlockDef } from '../shared/types'
import type { Entry, ListOpts } from './repo'
import { can } from '../../auth/shared/can'

export type { ContentType, Entry, BlockDef, Viewer }
export { compileFields } from './compile'
export { renderMarkdown, collectRefs, sanitizeHtml, escapeHtml, embedHtml } from './markdown.js'
export { formToObject } from './walk.js'
export { slugify } from './service'

svc := (c: Container) => c.resolve('content.service') as ContentService

// The content facade: routes and other features call this, never the
// service or the repo directly. Everything that touches the database is
// async — await it.
export cms := (c: Container) => {
	types: () => svc(c).types()
	getType: (id: string) => svc(c).getType(id)
	fields: (ct: ContentType) => svc(c).fields(ct)
	compile: (fields: any[]) => svc(c).compile(fields)
	form: (typeId: string) => svc(c).form(typeId)
	optionsFor: (key: string) => svc(c).optionsFor(key)
	list: (typeId: string | null, opts?: ListOpts) => svc(c).list(typeId, opts)
	count: (typeId: string | null, opts?: ListOpts | string) => svc(c).count(typeId, opts)
	get: (typeId: string, id: number) => svc(c).get(typeId, id)
	byId: (id: number) => svc(c).byId(id)
	recent: (limit?: number) => svc(c).recent(limit)
	save: (typeId: string, id: number | null, raw: Record<string, any>, user: any) => svc(c).save(typeId, id, raw, user)
	setStatus: (typeId: string, id: number, status: string, opts?: { publishAt?: number | null, user?: any }) => svc(c).setStatus(typeId, id, status, opts)
	remove: (typeId: string, id: number, user?: any) => svc(c).remove(typeId, id, user)
	revisions: (entryId: number) => svc(c).revisions(entryId)
	revision: (entryId: number, revId: number) => svc(c).revision(entryId, revId)
	restore: (typeId: string, id: number, revId: number, user: any) => svc(c).restore(typeId, id, revId, user)
	rerender: (ids?: number[]) => svc(c).rerender(ids)
	present: (ct: ContentType, entry: Entry, viewer?: Viewer) => svc(c).present(ct, entry, viewer)
	htmlOf: (entry: Entry) => svc(c).htmlOf(entry)
	urlFor: (ct: ContentType, entry: { slug: string }) => svc(c).urlFor(ct, entry)
	resolve: (path: string, viewer?: Viewer | boolean) => svc(c).resolve(path, viewer)
	query: (typeId: string, opts?: ListOpts & { member?: boolean }) => svc(c).query(typeId, opts)
	lookupMedia: (ids: number[]) => svc(c).lookupMedia(ids)
	mediaRefs: (ct: ContentType, data: Record<string, any>) => svc(c).mediaRefs(ct, data)
	reassignAuthor: (fromId: number, toId: number | null) => (c.resolve('content.repo') as any).reassignAuthor(fromId, toId)
	nav: () => svc(c).nav()
	adminNav: (user?: any) => svc(c).adminNav(user)
	counts: () => svc(c).counts()
	blocks: () => (c.resolve('content.registry') as any).blocks()
}

// Who may edit an entry: anyone with content.edit_any, or its author with
// content.edit_own while it is not published.
export canEditEntry := (user: any, entry: Entry | null | undefined) =>
	return can(user, 'content.create') unless entry
	return true if can(user, 'content.edit_any')
	can(user, 'content.edit_own') && entry.author_id === user?.id && entry.status !== 'published'

// --- extension points (called from a feature's module.c) --------------------

// Content types are bindings on the container, so registration order never
// matters.
export registerType := (app: Container, ct: ContentType) =>
	app.single `cms.type:${ct.id}`, => ct

// Extra fields added to every type the group applies to, on their own tab:
// { id, label, tab?, appliesTo?(ct), fields }
export registerFieldGroup := (app: Container, group: any) =>
	app.single `cms.fieldgroup:${group.id}`, => group

// A page block: { id, label, icon, fields, resolve?(props, { c, viewer, entry }) }.
// `resolve` runs per request and its result reaches the block component as `data`.
export registerBlock := (app: Container, block: BlockDef) =>
	app.single `cms.block:${block.id}`, => block

// Lifecycle hooks: beforeSave (may return replacement data), afterSave,
// status, slug, delete. fn receives { c, ct, entry?, prev?, data?, user, … }.
export registerHook := (app: Container, event: string, id: string, fn: (ctx: any) => any) =>
	app.single `cms.hook:${event}:${id}`, => fn

// Dynamic options for select-like fields: fn(c) → [{ value, label }].
export registerOptions := (app: Container, key: string, fn: (c: Container) => Promise<any[]>) =>
	app.single `cms.options:${key}`, => fn

// Admin navigation: { id, label, icon, href, section, order?, capability? }.
export registerAdminNav := (app: Container, item: any) =>
	app.single `cms.nav:${item.id}`, => item
