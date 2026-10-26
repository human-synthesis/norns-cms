import * as v from 'valibot'
import type { Container } from '@human-synthesis/norns/server'
import type { ContentType, FieldDef } from '../shared/types'
import { STATUSES } from '../shared/types'
import type { TypeRegistry } from './registry'
import type { EntriesRepo, Entry, ListOpts } from './repo'
import { compileFields } from './compile'
import { renderMarkdown, collectRefs } from './markdown.js'
import { markdownSources, mediaIds, renderFields } from './walk.js'
import { can } from '../../auth/shared/can'

export slugify := (s: string) =>
	s.toLowerCase()
		.normalize('NFKD')
		.replace(/[̀-ͯ]/g, '')
		.replace(/[^a-z0-9/]+/g, '-')
		.replace(/\/+/g, '/')
		.replace(/-*\/-*/g, '/')
		.replace(/^[-/]+|[-/]+$/g, '')

export type SaveResult = { ok: true, id: number, entry: Entry } | { ok: false, issues: any[] }

export type Viewer =
	preview?: boolean       // staff: drafts and scheduled entries resolve
	member?: boolean        // may read members-only entries

// Compiled schemas, keyed by type id. Module-level because the service is a
// transient binding (one per resolve) while type definitions never change
// after boot.
compiledTypes := new Map<string, any>()

export class ContentService
	registry: TypeRegistry
	repo: EntriesRepo
	c: Container
	constructor(@registry: TypeRegistry, @repo: EntriesRepo, @c: Container)

	// --- types -------------------------------------------------------------

	types(): ContentType[]
		@registry.all()

	getType(id: string): ContentType | undefined
		@registry.get id

	mustType(id: string): ContentType
		ct := @registry.get id
		unless ct
			throw new Error(`content: unknown type "${id}"`)
		ct

	fields(ct: ContentType): FieldDef[]
		@registry.fieldsFor ct

	blockLookup = (id: string) => @registry.block id

	compiled(ct: ContentType)
		hit := compiledTypes.get ct.id
		return hit if hit
		built := { ...compileFields(@fields(ct), { block: @blockLookup }), titleField: ct.titleField ?? 'title' }
		compiledTypes.set ct.id, built
		built

	compile(fields: FieldDef[])
		compileFields fields, { block: @blockLookup }

	// Option lists for fields whose choices live in the database:
	// `reference` (entries of `to`), `term` (a taxonomy), `users`, or any
	// field naming a provider in `optionsFrom`.
	optionKey(f: FieldDef): string | null
		return f.optionsFrom if f.optionsFrom
		return `entries:${f.to}` if f.type === 'reference' || f.type === 'references'
		return `terms:${f.taxonomy}` if f.type === 'term'
		return 'users:staff' if f.type === 'users'
		null

	async withOptions(fields: FieldDef[]): Promise<FieldDef[]>
		out := [] as FieldDef[]
		for f of fields
			key := @optionKey f
			if f.type === 'repeater'
				out.push { ...f, fields: await @withOptions(f.fields ?? []) }
			else if key
				out.push { ...f, options: await @optionsFor(key) }
			else
				out.push f
		out

	async optionsFor(key: string): Promise<{ value: any, label: string }[]>
		provider := @registry.options key
		return await provider(@c) if provider
		if key.startsWith('entries:')
			typeId := key.slice(8)
			entries := await @repo.list typeId, { limit: 500, order: 'title' }
			return entries.map (e) => ({ value: e.id, label: e.title || e.slug })
		[]

	// Everything the generic editor needs, with dynamic options resolved and
	// block definitions stripped of server functions.
	async form(typeId: string)
		ct := @mustType typeId
		blocks := [] as any[]
		for b of @registry.blocks()
			blocks.push { id: b.id, label: b.label, icon: b.icon, description: b.description ?? '', fields: await @withOptions(b.fields) }
		{
			fields: await @withOptions(@fields(ct))
			tabs: @registry.tabsFor(ct)
			blocks
		}

	// --- reads -------------------------------------------------------------

	list(typeId: string | null, opts: ListOpts = {}): Promise<Entry[]>
		@repo.list (typeId ? @mustType(typeId).id : null), opts

	count(typeId: string | null, opts: ListOpts | string = {}): Promise<number>
		o := typeof opts === 'string' ? { status: opts } : opts
		@repo.count (typeId ? @mustType(typeId).id : null), o

	get(typeId: string, id: number): Promise<Entry | undefined>
		@repo.get @mustType(typeId).id, id

	byId(id: number): Promise<Entry | undefined>
		@repo.byId id

	recent(limit = 10): Promise<Entry[]>
		@repo.recent limit

	// --- writes ------------------------------------------------------------

	async runHooks(event: string, ctx: Record<string, any>)
		for hook of @registry.hooks event
			result := await hook({ c: @c, ...ctx })
			if event === 'beforeSave' && result && typeof result === 'object'
				ctx.data = result
		ctx

	async save(typeId: string, id: number | null, raw: Record<string, any>, user: any): Promise<SaveResult>
		ct := @mustType typeId
		{ schema, titleField } := @compiled ct
		result := v.safeParse schema, raw
		unless result.success
			return { ok: false, issues: result.issues }
		prev := id ? await @repo.get(ct.id, id) : undefined
		if id && !prev
			return { ok: false, issues: [{ message: 'Entry not found', path: [{ key: 'title' }] }] }
		hookCtx := await @runHooks 'beforeSave', { ct, data: result.output, prev, user, raw }
		data := hookCtx.data as Record<string, any>
		title := String(data[titleField] ?? '')
		requested := String(raw.slug ?? '').trim()
		slug := await @uniqueSlug ct.id, slugify(requested || title) || 'untitled', id
		visibility := raw.visibility === 'members' ? 'members' : (raw.visibility === 'public' ? 'public' : (prev?.visibility ?? 'public'))
		rendered := await @renderData ct, data
		userId := user?.id ?? null
		entryId .= id ?? 0
		if id
			await @repo.update id, { title, slug, data, rendered, visibility }
		else
			entryId = await @repo.insert { type: ct.id, title, slug, data, rendered, visibility, authorId: userId }
		await @repo.addRevision entryId, title, slug, data, userId
		entry := (await @repo.get ct.id, entryId) as Entry
		await @runHooks 'afterSave', { ct, entry, prev, user }
		if prev && prev.slug !== slug && prev.status === 'published'
			await @runHooks 'slug', { ct, entry, fromUrl: @urlFor(ct, prev), toUrl: @urlFor(ct, entry), user }
		{ ok: true, id: entryId, entry }

	async uniqueSlug(typeId: string, base: string, excludeId: number | null): Promise<string>
		candidate .= base
		n .= 2
		while await @repo.slugTaken(typeId, candidate, excludeId)
			candidate = `${base}-${n}`
			n++
		candidate

	// status: draft | in_review | published. publishAt (ms) schedules a
	// publish in the future; null publishes now.
	async setStatus(typeId: string, id: number, status: string, opts: { publishAt?: number | null, user?: any } = {}): Promise<Entry | undefined>
		ct := @mustType typeId
		prev := await @repo.get ct.id, id
		return undefined unless prev
		next := STATUSES.includes(status) ? status : 'draft'
		await @repo.setStatus id, next, next === 'published' ? (opts.publishAt ?? null) : null
		entry := await @repo.get ct.id, id
		await @runHooks 'status', { ct, entry, from: prev.status, to: next, user: opts.user }
		entry

	async remove(typeId: string, id: number, user: any = null): Promise<void>
		ct := @mustType typeId
		entry := await @repo.get ct.id, id
		return unless entry
		await @repo.remove id
		await @runHooks 'delete', { ct, entry, user }
		return

	revisions(entryId: number)
		@repo.revisions entryId

	revision(entryId: number, revId: number)
		@repo.revision entryId, revId

	// Restoring re-saves the old values through the normal save path, so the
	// restore itself is validated and becomes the newest revision.
	async restore(typeId: string, id: number, revId: number, user: any): Promise<SaveResult>
		rev := await @repo.revision id, revId
		unless rev
			return { ok: false, issues: [{ message: 'Revision not found' }] }
		raw := { ...rev.data } as Record<string, any>
		for f of @fields(@mustType typeId)
			value := raw[f.name]
			if f.type === 'blocks' || f.type === 'repeater' || f.type === 'json'
				raw[f.name] = JSON.stringify(value ?? (f.type === 'json' ? null : []))
		raw.slug = rev.slug
		@save typeId, id, raw, user

	// --- rendering ---------------------------------------------------------

	// Markdown → cached HTML at save time, with media and article references
	// looked up first (the renderer itself is synchronous).
	async renderData(ct: ContentType, data: Record<string, any>)
		fields := @fields ct
		sources := markdownSources fields, data, @blockLookup
		mediaRefs := new Set<number>()
		slugRefs := new Set<string>()
		for src of sources
			refs := collectRefs src
			for m of refs.media
				mediaRefs.add m
			for slug of refs.articles
				slugRefs.add slug
		ctx := { media: await @lookupMedia([...mediaRefs]), articles: {} as Record<string, any> }
		articleType := @registry.get 'article'
		if articleType && slugRefs.size
			for e of await @repo.bySlugs('article', [...slugRefs])
				ctx.articles[e.slug] = { title: e.title, url: @urlFor(articleType, e), summary: e.data.summary ?? '' }
		renderFields fields, data, @blockLookup, (md: string) => renderMarkdown(md, ctx)

	async lookupMedia(ids: number[]): Promise<Record<string, any>>
		return {} unless ids.length
		return {} unless @c.has 'content.mediaResolver'
		resolver := @c.resolve('content.mediaResolver') as (ids: number[]) => Promise<Record<string, any>>
		await resolver ids

	// Re-render cached HTML, e.g. after media alt text changed or a
	// renderer upgrade. Without ids: every entry.
	async rerender(ids?: number[]): Promise<number>
		list := ids ?? (await @repo.allIds()).map((r) => r.id)
		n .= 0
		for id of list
			entry := await @repo.byId id
			continue unless entry
			ct := @registry.get entry.type
			continue unless ct
			await @repo.setRendered id, await @renderData(ct, entry.data)
			n++
		n

	// Request-time presentation: media objects for image fields and blocks,
	// and block resolvers (article lists, locations, cookie tables …).
	async present(ct: ContentType, entry: Entry, viewer: Viewer = {})
		fields := @fields ct
		media := await @lookupMedia mediaIds(fields, entry.data, @blockLookup)
		blocks := [] as any[]
		for f of fields
			continue unless f.type === 'blocks'
			list := (entry.data[f.name] ?? []) as any[]
			renderedList := (entry.rendered?.[f.name] ?? []) as any[]
			for block, i of list
				def := @registry.block block.type
				continue unless def
				extra := def.resolve ? await def.resolve(block.props, { c: @c, viewer, entry }) : null
				blocks.push { id: block.id, type: block.type, props: block.props, html: renderedList[i] ?? {}, data: extra }
		{ media, blocks }

	// Media ids an entry references: image fields plus media: links in its
	// markdown (for usage tracking).
	mediaRefs(ct: ContentType, data: Record<string, any>): number[]
		fields := @fields ct
		ids := new Set<number>(mediaIds(fields, data, @blockLookup))
		for src of markdownSources(fields, data, @blockLookup)
			for m of collectRefs(src).media
				ids.add m
		[...ids]

	urlFor(ct: ContentType, entry: { slug: string }): string
		`${ct.basePath || ''}/${entry.slug}`

	// --- public routing ----------------------------------------------------

	locked(entry: Entry, viewer: Viewer)
		entry.visibility === 'members' && !viewer.member && !viewer.preview

	// `/articles` → list (listable types), `/articles/<slug>` → entry,
	// `/<slug>` → page. Drafts and scheduled entries resolve only in preview;
	// members-only entries resolve for everyone but come back `locked` with
	// their body removed unless the viewer is a member.
	async resolve(path: string, viewer: Viewer | boolean = {})
		vw := typeof viewer === 'boolean' ? { preview: viewer } : viewer
		segments := path.split('/').filter(Boolean)
		for ct of @registry.all()
			base := String(ct.basePath || '').replace(/^\//, '')
			if base
				continue unless segments[0] === base
				if segments.length === 1
					if ct.listable
						entries := await @repo.list ct.id, { status: 'live', limit: 100, order: 'published' }
						return { kind: 'list', ct, entries }
					continue
				entry := await @repo.bySlug ct.id, segments.slice(1).join('/'), !vw.preview
				if entry
					return await @entryResult ct, entry, vw
			else
				continue if segments.length === 0
				entry := await @repo.bySlug ct.id, segments.join('/'), !vw.preview
				if entry
					return await @entryResult ct, entry, vw
		null

	async entryResult(ct: ContentType, entry: Entry, viewer: Viewer)
		locked := @locked entry, viewer
		shown := locked ? { ...entry, rendered: {}, data: @teaser(ct, entry) } : entry
		presented := locked ? { media: await @lookupMedia(mediaIds(@fields(ct), shown.data, @blockLookup)), blocks: [] } : await @present(ct, entry, viewer)
		{ kind: 'entry', ct, entry: shown, html: @htmlOf(shown), locked, ...presented, url: @urlFor(ct, entry) }

	// Locked entries keep only what is safe to show: title, summary-ish
	// fields, cover image and SEO fields.
	teaser(ct: ContentType, entry: Entry)
		keep := new Set(['title', 'summary', 'excerpt', 'cover', 'coverAlt', ...(ct.teaserFields ?? [])])
		out := {} as Record<string, any>
		for [k, value] of Object.entries(entry.data)
			out[k] = value if keep.has(k) || k.startsWith('seo')
		out

	// Back-compat shape for templates: { field: html }.
	htmlOf(entry: Entry): Record<string, string>
		out := {} as Record<string, string>
		for [k, value] of Object.entries(entry.rendered ?? {})
			out[k] = value.html if value && typeof value === 'object' && !Array.isArray(value) && 'html' in value
		out

	// Public list query for any type: live entries, members-only ones only
	// for members.
	async query(typeId: string, opts: ListOpts & { member?: boolean } = {})
		ct := @mustType typeId
		o := { ...opts, status: 'live' }
		o.visibility = 'public' unless opts.member
		entries := await @repo.list ct.id, { order: 'published', ...o }
		total := await @repo.count ct.id, o
		{ ct, entries, total }

	// Public navigation: pages flagged showInNav (ordered by navOrder, then
	// title) and listable types by base path.
	async nav(): Promise<{ label: string, href: string }[]>
		pages := [] as any[]
		types := [] as { label: string, href: string }[]
		for ct of @registry.all()
			if ct.basePath
				types.push { label: ct.labelPlural, href: ct.basePath } if ct.listable
			else
				for e of await @repo.list(ct.id, { status: 'live', limit: 100, order: 'title' })
					pages.push e if e.data.showInNav
		pages.sort (a, b) => (Number(a.data.navOrder ?? 0) - Number(b.data.navOrder ?? 0)) || a.title.localeCompare(b.title)
		[...pages.map((e) => ({ label: e.data.navLabel || e.title, href: `/${e.slug}` })), ...types]

	// Admin navigation as a tree: sections → items → children. Content
	// types become items with "All …" / "New …" children; any feature adds
	// items (or children, with `parent`) through registerAdminNav. Items
	// the user lacks the capability for are dropped; a parent left with no
	// visible children and no link of its own disappears too.
	adminNav(user: any = null)
		items := [] as any[]
		for ct, i of @registry.all()
			id := `type-${ct.id}`
			items.push { id, label: ct.labelPlural, icon: ct.icon ?? 'lucide:file-text', section: 'Content', order: 10 + i, capability: 'content.view' }
			items.push { id: `${id}-all`, parent: id, label: `All ${ct.labelPlural.toLowerCase()}`, href: `/admin/content/${ct.id}`, order: 1 }
			items.push { id: `${id}-review`, parent: id, label: 'Waiting for review', href: `/admin/content/${ct.id}?status=in_review`, order: 2, capability: 'content.publish' }
			items.push { id: `${id}-new`, parent: id, label: `New ${ct.label.toLowerCase()}`, href: `/admin/content/${ct.id}/new`, order: 3, capability: 'content.create' }
		items.push ...@registry.nav()
		allowed := (it: any) => !user || !it.capability || can(user, it.capability)
		byOrder := (a: any, b: any) => (a.order ?? 100) - (b.order ?? 100)
		top := [] as any[]
		for it of items.filter((x) => !x.parent && allowed(x)).sort(byOrder)
			children := items.filter((x) => x.parent === it.id && allowed(x)).sort(byOrder).map((x) => ({ id: x.id, label: x.label, href: x.href, icon: x.icon }))
			continue if !it.href && children.length === 0
			top.push { id: it.id, label: it.label, href: it.href ?? null, icon: it.icon ?? 'lucide:circle', section: it.section ?? 'Content', children }
		SECTIONS := ['Content', 'People', 'Marketing', 'System']
		sections := [...SECTIONS, ...new Set(top.map((t) => t.section).filter((s) => !SECTIONS.includes(s)))]
		sections.map((name) => ({ section: name, items: top.filter((t) => t.section === name) })).filter((g) => g.items.length)

	async counts()
		out := [] as { ct: ContentType, total: number, published: number, review: number }[]
		for ct of @registry.all()
			total := await @repo.count ct.id
			published := await @repo.count ct.id, { status: 'live' }
			review := await @repo.count ct.id, { status: 'in_review' }
			out.push { ct, total, published, review }
		out
