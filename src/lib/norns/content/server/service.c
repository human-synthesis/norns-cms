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
import { getScope } from '@human-synthesis/norns/server'

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

	// --- languages -----------------------------------------------------------

	i18n(): any
		@c.has('i18n.service') ? @c.resolve('i18n.service') : null

	languages(): string[]
		@i18n()?.contentLanguages() ?? ['en']

	defaultLanguage(): string
		@i18n()?.defaultLanguage() ?? 'en'

	// The request's locale (public pages), else the default language.
	current()
		loc := getScope()?.event?.locals?.locale
		{ language: loc?.language ?? @defaultLanguage(), market: loc?.market?.code ?? null, contentFallback: !!loc?.market?.contentFallback, chain: loc?.chain ?? [loc?.language ?? @defaultLanguage()] }

	marketCodes(): string[]
		(@i18n()?.markets() ?? []).map((m: any) => m.code)

	normalizeMarkets(raw: any, prev: string[] | undefined): string[]
		return prev ?? [] if raw === undefined
		list := Array.isArray(raw) ? raw : String(raw ?? '').split(',')
		known := @marketCodes()
		[...new Set(list.map((x: any) => String(x).trim()).filter((x: string) => x && known.includes(x)))]

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

	// extra (translations): { locale, group, translationOf, sourceUpdatedAt }.
	// An entry's language is fixed when it is created.
	async save(typeId: string, id: number | null, raw: Record<string, any>, user: any, extra: Record<string, any> = {}): Promise<SaveResult>
		ct := @mustType typeId
		{ schema, titleField } := @compiled ct
		result := v.safeParse schema, raw
		unless result.success
			return { ok: false, issues: result.issues }
		prev := id ? await @repo.get(ct.id, id) : undefined
		if id && !prev
			return { ok: false, issues: [{ message: 'Entry not found', path: [{ key: 'title' }] }] }
		locale := prev?.locale ?? (String(extra.locale ?? raw.locale ?? '') || @defaultLanguage())
		unless prev || @languages().includes(locale)
			return { ok: false, issues: [{ message: `No localisation offers the language "${locale}"`, path: [{ key: 'locale' }] }] }
		hookCtx := await @runHooks 'beforeSave', { ct, data: result.output, prev, user, raw, locale }
		data := hookCtx.data as Record<string, any>
		title := String(data[titleField] ?? '')
		requested := String(raw.slug ?? '').trim()
		slug := await @uniqueSlug ct.id, locale, slugify(requested || title) || 'untitled', id
		visibility := raw.visibility === 'members' ? 'members' : (raw.visibility === 'public' ? 'public' : (prev?.visibility ?? 'public'))
		markets := @normalizeMarkets raw.markets, prev?.markets
		rendered := await @renderData ct, data, locale
		userId := user?.id ?? null
		entryId .= id ?? 0
		if id
			await @repo.update id, { title, slug, data, rendered, visibility, markets }
		else
			entryId = await @repo.insert { type: ct.id, locale, title, slug, data, rendered, visibility, markets, authorId: userId, group: extra.group ?? null, translationOf: extra.translationOf ?? null, sourceUpdatedAt: extra.sourceUpdatedAt ?? null }
		await @repo.addRevision entryId, title, slug, data, userId
		entry := (await @repo.get ct.id, entryId) as Entry
		await @runHooks 'afterSave', { ct, entry, prev, user }
		if prev && prev.slug !== slug && prev.status === 'published'
			for place of @placesOf(prev.locale)
				await @runHooks 'slug', { ct, entry, fromUrl: place.base + @pathIn(ct, prev), toUrl: place.base + @pathIn(ct, entry), user }
		{ ok: true, id: entryId, entry }

	// Raw form values from stored data, for re-saving through the normal path.
	rawFrom(ct: ContentType, data: Record<string, any>)
		raw := { ...data } as Record<string, any>
		for f of @fields(ct)
			if f.type === 'blocks' || f.type === 'repeater' || f.type === 'json'
				raw[f.name] = JSON.stringify(data[f.name] ?? (f.type === 'json' ? null : []))
		raw

	// A new entry in another language, linked to the source's group and
	// stamped with the source's version. `data` overrides the copied
	// values (e.g. a machine translation); the result is always a draft.
	async translate(typeId: string, sourceId: number, locale: string, user: any, data: Record<string, any> | null = null): Promise<SaveResult>
		ct := @mustType typeId
		source := await @repo.get ct.id, sourceId
		return { ok: false, issues: [{ message: 'Entry not found' }] } unless source
		return { ok: false, issues: [{ message: `No localisation offers "${locale}"` }] } unless @languages().includes(locale)
		siblings := await @repo.group(source.translation_group ?? source.id)
		existing := siblings.find((e) => e.locale === locale)
		return { ok: false, issues: [{ message: 'That translation exists already' }], existing: existing.id } if existing
		root := siblings.find((e) => !e.translation_of) ?? source
		raw := { ...@rawFrom(ct, data ?? source.data), slug: source.slug, visibility: source.visibility, markets: source.markets }
		@save ct.id, null, raw, user, { locale, group: source.translation_group ?? source.id, translationOf: root.id, sourceUpdatedAt: root.updated_at }

	// The other languages of an entry, with their state.
	async translations(entry: Entry)
		group := await @repo.group(entry.translation_group ?? entry.id)
		root := group.find((e) => !e.translation_of) ?? group[0]
		group.map (e) => ({
			id: e.id
			locale: e.locale
			title: e.title
			slug: e.slug
			status: e.status
			published_at: e.published_at
			updated_at: e.updated_at
			isSource: e.id === root?.id
			outdated: !!(e.translation_of && root && e.id !== root.id && (e.source_updated_at ?? 0) < root.updated_at)
		})

	// "I have brought this translation up to date with its source."
	async markCurrent(entry: Entry)
		group := await @repo.group(entry.translation_group ?? entry.id)
		root := group.find((e) => !e.translation_of)
		await @repo.stamp(entry.id, root.updated_at) if root && root.id !== entry.id
		return

	// Translation coverage for lists: group id → locales present.
	async coverage(entries: Entry[])
		rows := await @repo.groups [...new Set(entries.map((e) => e.translation_group ?? e.id))]
		out := {} as Record<string, string[]>
		for r of rows
			key := String(r.translation_group)
			out[key] = [...(out[key] ?? []), r.locale]
		out

	localeCounts(typeId: string)
		@repo.localeCounts @mustType(typeId).id

	async uniqueSlug(typeId: string, locale: string, base: string, excludeId: number | null): Promise<string>
		candidate .= base
		n .= 2
		while await @repo.slugTaken(typeId, locale, candidate, excludeId)
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
		raw := @rawFrom @mustType(typeId), rev.data
		raw.slug = rev.slug
		@save typeId, id, raw, user

	// --- rendering ---------------------------------------------------------

	// Markdown → cached HTML at save time, with media and article references
	// looked up first (the renderer itself is synchronous).
	async renderData(ct: ContentType, data: Record<string, any>, locale = 'en')
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
		strings := @i18n() ? await @i18n().strings(locale) : {}
		ctx := { media: await @lookupMedia([...mediaRefs]), articles: {} as Record<string, any>, strings }
		articleType := @registry.get 'article'
		if articleType && slugRefs.size
			for e of await @repo.bySlugs('article', [...slugRefs], locale)
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
			await @repo.setRendered id, await @renderData(ct, entry.data, entry.locale)
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

	// Path of an entry inside its locale: type section (in the entry's
	// language) + slug.
	pathIn(ct: ContentType, entry: { slug: string, locale?: string }): string
		locale := entry.locale ?? @defaultLanguage()
		seg := @i18n() ? @i18n().typeSegment(locale, ct.id, ct.basePath) : String(ct.basePath || '').replace(/^\//, '')
		(seg ? `/${seg}` : '') + `/${entry.slug}`

	// Every localisation base where a language is shown.
	placesOf(locale: string)
		@i18n()?.placesOf(locale) ?? [{ origin: '', base: '', hreflang: [locale], market: null }]

	// Site path of an entry (locale base + path), in the current request's
	// localisation when it offers the entry's language.
	urlFor(ct: ContentType, entry: { slug: string, locale?: string }): string
		locale := entry.locale ?? @defaultLanguage()
		base := @i18n()?.link(locale).base ?? ''
		base + @pathIn(ct, entry)

	absoluteUrlFor(ct: ContentType, entry: { slug: string, locale?: string }): string
		locale := entry.locale ?? @defaultLanguage()
		l := @i18n()?.link(locale)
		(l?.origin ?? '') + (l?.base ?? '') + @pathIn(ct, entry)

	// Section path of a listable type in a language (/artikel).
	sectionPath(ct: ContentType, locale: string): string
		seg := @i18n() ? @i18n().typeSegment(locale, ct.id, ct.basePath) : String(ct.basePath || '').replace(/^\//, '')
		seg ? `/${seg}` : ''

	// --- public routing ----------------------------------------------------

	locked(entry: Entry, viewer: Viewer)
		entry.visibility === 'members' && !viewer.member && !viewer.preview

	available(entry: Entry, market: string | null)
		!market || !entry.markets?.length || entry.markets.includes(market)

	// `rest` (the path inside the locale base) → a list, an entry, or null.
	// Languages: the request's, then — when its localisation allows
	// content fallback — its fallback chain.
	async resolve(path: string, viewer: Viewer | boolean = {}, where: any = null)
		vw := typeof viewer === 'boolean' ? { preview: viewer } : viewer
		loc := where ?? @current()
		segments := path.split('/').filter(Boolean)
		chain := loc.contentFallback ? (loc.chain ?? [loc.language]) : [loc.language]
		for ct of @registry.all()
			base := @sectionPath(ct, loc.language).replace(/^\//, '')
			if base
				continue unless segments[0] === base
				if segments.length === 1
					if ct.listable
						entries := await @repo.list ct.id, { status: 'live', limit: 100, order: 'published', locale: loc.language, market: loc.market ?? undefined }
						return { kind: 'list', ct, entries }
					continue
			else
				continue if segments.length === 0
			for lang, i of chain
				entry := await @repo.bySlug ct.id, (base ? segments.slice(1).join('/') : segments.join('/')), !vw.preview, lang
				if entry && @available(entry, loc.market)
					return await @entryResult ct, entry, vw, (i > 0 ? lang : null)
		null

	// Published translations of an entry, for hreflang and the language
	// switcher: [{ locale, entry }] (members-only and unavailable ones out).
	async alternatesOf(ct: ContentType, entry: Entry)
		group := await @repo.group(entry.translation_group ?? entry.id)
		now := Date.now()
		group.filter((e) => e.status === 'published' && (!e.published_at || e.published_at <= now)).map (e) => ({ locale: e.locale, slug: e.slug, markets: e.markets, visibility: e.visibility, path: @pathIn(ct, e), updated_at: e.updated_at })

	async entryResult(ct: ContentType, entry: Entry, viewer: Viewer, fallbackFrom: string | null = null)
		locked := @locked entry, viewer
		shown := locked ? { ...entry, rendered: {}, data: @teaser(ct, entry) } : entry
		presented := locked ? { media: await @lookupMedia(mediaIds(@fields(ct), shown.data, @blockLookup)), blocks: [] } : await @present(ct, entry, viewer)
		{ kind: 'entry', ct, entry: shown, html: @htmlOf(shown), locked, ...presented, url: @urlFor(ct, entry), alternates: await @alternatesOf(ct, entry), fallbackFrom }

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

	// Public list query: live entries in a language (default: the
	// request's), available in a localisation, members-only ones only for
	// members.
	async query(typeId: string, opts: ListOpts & { member?: boolean, anyLocale?: boolean } = {})
		ct := @mustType typeId
		loc := @current()
		{ member, anyLocale, ...rest } := opts
		o := { ...rest, status: 'live' } as ListOpts
		o.visibility = 'public' unless member
		o.locale = opts.locale ?? (anyLocale ? undefined : loc.language)
		o.market = opts.market ?? (anyLocale ? undefined : (loc.market ?? undefined))
		entries := await @repo.list ct.id, { order: 'published', ...o }
		total := await @repo.count ct.id, o
		{ ct, entries, total }

	// Public navigation in the request's language: pages flagged showInNav
	// (ordered by navOrder, then title) and listable types by section path.
	async nav(): Promise<{ label: string, href: string, typeId?: string }[]>
		loc := @current()
		base := @i18n()?.link(loc.language).base ?? ''
		pages := [] as any[]
		types := [] as { label: string, href: string, typeId?: string }[]
		for ct of @registry.all()
			section := @sectionPath ct, loc.language
			if section
				types.push { label: ct.labelPlural, href: base + section, typeId: ct.id } if ct.listable
			else
				for e of await @repo.list(ct.id, { status: 'live', limit: 100, order: 'title', locale: loc.language, market: loc.market ?? undefined })
					pages.push e if e.data.showInNav
		pages.sort (a, b) => (Number(a.data.navOrder ?? 0) - Number(b.data.navOrder ?? 0)) || a.title.localeCompare(b.title)
		[...pages.map((e) => ({ label: e.data.navLabel || e.title, href: `${base}/${e.slug}` })), ...types]

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
