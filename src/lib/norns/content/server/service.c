import * as v from 'valibot'
import { marked } from 'marked'
import type { ContentType } from '../shared/types'
import type { TypeRegistry } from './registry'
import type { EntriesRepo, Entry, ListOpts } from './repo'
import { compileType } from './compile'

export slugify := (s: string) =>
	s.toLowerCase()
		.normalize('NFKD')
		.replace(/[̀-ͯ]/g, '')
		.replace(/[^a-z0-9]+/g, '-')
		.replace(/^-+|-+$/g, '')

export type SaveResult = { ok: true, id: number } | { ok: false, issues: any[] }

export class ContentService
	registry: TypeRegistry
	repo: EntriesRepo
	cache: Map<string, any> = new Map()
	constructor(@registry: TypeRegistry, @repo: EntriesRepo)

	types(): ContentType[]
		@registry.all()

	getType(id: string): ContentType | undefined
		@registry.get id

	mustType(id: string): ContentType
		ct := @registry.get id
		unless ct
			throw new Error(`content: unknown type "${id}"`)
		ct

	compiled(ct: ContentType)
		hit := @cache.get ct.id
		return hit if hit
		built := compileType ct
		@cache.set ct.id, built
		built

	form(typeId: string)
		ct := @mustType typeId
		{ fields: ct.fields }

	list(typeId: string, opts: ListOpts = {}): Entry[]
		@repo.list @mustType(typeId).id, opts

	count(typeId: string, status?: string): number
		@repo.count @mustType(typeId).id, status

	get(typeId: string, id: number): Entry | undefined
		@repo.get @mustType(typeId).id, id

	recent(limit = 10): Entry[]
		@repo.recent limit

	save(typeId: string, id: number | null, raw: Record<string, any>, userId: number | null): SaveResult
		ct := @mustType typeId
		{ schema, titleField } := @compiled ct
		result := v.safeParse schema, raw
		unless result.success
			return { ok: false, issues: result.issues }
		data := result.output as Record<string, any>
		title := String(data[titleField] ?? '')
		requested := String(raw.slug ?? '').trim()
		slug := @uniqueSlug ct.id, slugify(requested || title) || 'untitled', id
		if id
			@repo.update id, { title, slug, data }
			@repo.addRevision id, data, userId
			return { ok: true, id }
		newId := @repo.insert { type: ct.id, title, slug, data, authorId: userId }
		@repo.addRevision newId, data, userId
		{ ok: true, id: newId }

	uniqueSlug(typeId: string, base: string, excludeId: number | null): string
		candidate .= base
		n .= 2
		while @repo.slugTaken(typeId, candidate, excludeId)
			candidate = `${base}-${n}`
			n++
		candidate

	setStatus(typeId: string, id: number, status: string): void
		@mustType typeId
		@repo.setStatus id, status === 'published' ? 'published' : 'draft'

	remove(typeId: string, id: number): void
		@mustType typeId
		@repo.remove id

	revisions(entryId: number)
		@repo.revisions entryId

	// Markdown fields → HTML, keyed by field name.
	render(ct: ContentType, entry: Entry): Record<string, string>
		out := {} as Record<string, string>
		for f of ct.fields
			if f.type === 'markdown'
				out[f.name] = marked.parse(String(entry.data[f.name] ?? ''), { async: false }) as string
		out

	urlFor(ct: ContentType, entry: Entry): string
		`${ct.basePath || ''}/${entry.slug}`

	// Public routing. `/posts` → list (when the type is listable),
	// `/posts/<slug>` → post, `/<slug>` → page. Drafts resolve only in preview.
	resolve(path: string, preview = false)
		segments := path.split('/').filter(Boolean)
		for ct of @registry.all()
			base := String(ct.basePath || '').replace(/^\//, '')
			if base
				continue unless segments[0] === base
				if segments.length === 1
					if ct.listable
						return { kind: 'list', ct, entries: @repo.list(ct.id, { status: 'published', limit: 100 }) }
					continue
				entry := @repo.bySlug ct.id, segments.slice(1).join('/'), !preview
				if entry
					return { kind: 'entry', ct, entry, html: @render(ct, entry) }
			else
				continue if segments.length === 0
				entry := @repo.bySlug ct.id, segments.join('/'), !preview
				if entry
					return { kind: 'entry', ct, entry, html: @render(ct, entry) }
		null

	// Public navigation: listable types by base path, plus pages flagged
	// showInNav.
	nav(): { label: string, href: string }[]
		out := [] as { label: string, href: string }[]
		for ct of @registry.all()
			if ct.basePath
				if ct.listable
					out.push { label: ct.labelPlural, href: ct.basePath }
			else
				for e of @repo.list(ct.id, { status: 'published', limit: 20 })
					if e.data.showInNav
						out.push { label: e.title, href: `/${e.slug}` }
		out

	adminNav(): { label: string, href: string, icon: string }[]
		items := [{ label: 'Dashboard', href: '/admin', icon: 'lucide:layout-dashboard' }]
		for ct of @registry.all()
			items.push { label: ct.labelPlural, href: `/admin/content/${ct.id}`, icon: ct.icon ?? 'lucide:file-text' }
		items

	counts()
		@registry.all().map((ct) => ({ ct, total: @repo.count(ct.id), published: @repo.count(ct.id, 'published') }))
