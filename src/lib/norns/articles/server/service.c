import type { Container } from '@human-synthesis/norns/server'
import type { TermsRepo, Term } from './repo'
import { cms, slugify } from '../../content/server/public'
import { auth } from '../../auth/server/public'
import { mail } from '../../mail/server/public'
import { siteOrigin, settings } from '../../settings/server/public'
import { readingMinutes, parseSources } from '../shared/types'

export TAXONOMIES := {
	category: { id: 'category', label: 'Category', labelPlural: 'Categories', hierarchical: true, base: '/articles/category' }
	tag: { id: 'tag', label: 'Tag', labelPlural: 'Tags', hierarchical: false, base: '/articles/tag' }
} as Record<string, any>

termSlug := (s: string) => slugify(s).replace(/\//g, '-') || 'term'

export class ArticlesService
	repo: TermsRepo
	c: Container
	constructor(@repo: TermsRepo, @c: Container)

	taxonomy(id: string)
		TAXONOMIES[id]

	terms(taxonomy: string)
		@repo.list taxonomy

	term(id: number)
		@repo.get id

	termBySlug(taxonomy: string, slug: string)
		@repo.bySlug taxonomy, slug

	termUrl(t: Term)
		`${TAXONOMIES[t.taxonomy]?.base ?? ''}/${t.slug}`

	async saveTerm(taxonomy: string, id: number | null, input: Record<string, any>)
		return { ok: false, error: 'Unknown taxonomy' } unless TAXONOMIES[taxonomy]
		name := String(input.name ?? '').trim()
		return { ok: false, error: 'Name is required', field: 'name' } unless name
		slug .= termSlug String(input.slug || name)
		prev := id ? await @repo.get(id) : undefined
		n .= 2
		base := slug
		while await @repo.slugTaken(taxonomy, slug, id ?? 0)
			slug = `${base}-${n}`
			n++
		parent := Number(input.parent_id) || null
		values := { slug, name, description: String(input.description ?? ''), parent_id: parent === id ? null : parent, sort: Number(input.sort) || 0 }
		if id
			await @repo.update id, values
			return { ok: true, id, prev, term: await @repo.get(id) }
		newId := await @repo.insert { taxonomy, ...values }
		{ ok: true, id: newId, prev: undefined, term: await @repo.get(newId) }

	async deleteTerm(id: number)
		await @repo.remove id
		return

	// Tags merge into another tag: entries move over (their data.tags names
	// are rewritten by re-saving through the normal path).
	async mergeTerm(fromId: number, intoId: number, user: any)
		from := await @repo.get fromId
		into := await @repo.get intoId
		return { ok: false, error: 'Pick two terms' } unless from && into && from.taxonomy === into.taxonomy && from.id !== into.id
		for entryId of await @repo.entriesOf(fromId)
			entry := await cms(@c).byId entryId
			continue unless entry
			data := { ...entry.data } as Record<string, any>
			if from.taxonomy === 'tag'
				data.tags = (data.tags ?? []).map((t: string) => t.toLowerCase() === from.name.toLowerCase() ? into.name : t)
			else if data.category === from.id
				data.category = into.id
			await @resave entry, data, user
		await @repo.remove fromId
		{ ok: true, from, into }

	async resave(entry: any, data: Record<string, any>, user: any)
		ct := cms(@c).getType entry.type
		return unless ct
		raw := { ...data, slug: entry.slug, visibility: entry.visibility } as Record<string, any>
		for f of cms(@c).fields(ct)
			if f.type === 'blocks' || f.type === 'repeater' || f.type === 'json'
				raw[f.name] = JSON.stringify(data[f.name] ?? [])
		await cms(@c).save ct.id, entry.id, raw, user
		return

	// Tag names → term ids, creating missing tags.
	async tagIds(names: string[]): Promise<number[]>
		ids := [] as number[]
		for name of names
			slug := termSlug name
			existing := await @repo.bySlug 'tag', slug
			ids.push existing ? existing.id : await @repo.insert({ taxonomy: 'tag', slug, name })
		ids

	async syncEntry(entry: any)
		tags := await @tagIds(entry.data.tags ?? [])
		await @repo.setEntryTerms entry.id, 'tag', tags
		cat := Number(entry.data.category) || 0
		await @repo.setEntryTerms entry.id, 'category', (cat ? [cat] : [])
		return

	// Everything a listing or article page shows besides the entry itself:
	// terms, authors, reading time, cover media.
	async decorate(entries: any[])
		return [] unless entries.length
		termRows := await @repo.entryTerms entries.map((e) => e.id)
		authorIds := new Set<number>()
		mediaIds := new Set<number>()
		for e of entries
			for a of (e.data.authors?.length ? e.data.authors : (e.author_id ? [e.author_id] : []))
				authorIds.add a
			mediaIds.add(Number(e.data.cover)) if Number(e.data.cover) > 0
		profiles := await auth(@c).profiles [...authorIds]
		media := await cms(@c).lookupMedia [...mediaIds]
		ct := cms(@c).getType 'article'
		entries.map (e) =>
			terms := termRows.filter((t: any) => t.entry_id === e.id)
			ids := e.data.authors?.length ? e.data.authors : (e.author_id ? [e.author_id] : [])
			{
				...e
				url: ct ? cms(@c).urlFor(ct, e) : `/articles/${e.slug}`
				category: terms.filter((t: any) => t.taxonomy === 'category').map((t: any) => ({ id: t.id, name: t.name, slug: t.slug, url: @termUrl(t) }))[0] ?? null
				tagTerms: terms.filter((t: any) => t.taxonomy === 'tag').map((t: any) => ({ id: t.id, name: t.name, slug: t.slug, url: @termUrl(t) }))
				authors: profiles.filter((p) => ids.includes(p.id)).map((p) => ({ ...p, url: p.slug ? `/authors/${p.slug}` : null }))
				coverMedia: media[e.data.cover] ?? null
				readingTime: e.data.readingTime ?? null
			}

	// Public article lists. featured entries first on the main list.
	async list(opts: { page?: number, perPage?: number, termId?: number, authorId?: number, member?: boolean, featured?: boolean } = {})
		perPage := opts.perPage ?? 10
		page := Math.max 1, opts.page ?? 1
		q := { limit: perPage, offset: (page - 1) * perPage, termId: opts.termId, featured: opts.featured, member: opts.member } as any
		{ entries, total } := await cms(@c).query 'article', q
		filtered := opts.authorId ? entries : entries
		{
			entries: await @decorate(filtered)
			total
			page
			pages: Math.max(1, Math.ceil(total / perPage))
		}

	async byAuthor(authorId: number, opts: { page?: number, member?: boolean } = {})
		// Authors live in data.authors (or author_id); filter in SQL on author_id
		// and in memory on the authors list, newest first.
		all := await cms(@c).query 'article', { limit: 500, member: opts.member }
		mine := all.entries.filter((e) => (e.data.authors?.length ? e.data.authors.includes(authorId) : e.author_id === authorId))
		perPage := 10
		page := Math.max 1, opts.page ?? 1
		{
			entries: await @decorate(mine.slice((page - 1) * perPage, page * perPage))
			total: mine.length
			page
			pages: Math.max(1, Math.ceil(mine.length / perPage))
		}

	async related(entry: any, limit = 3)
		cat := Number(entry.data.category) || 0
		return [] unless cat
		{ entries } := await cms(@c).query 'article', { termId: cat, limit, excludeIds: [entry.id] }
		await @decorate entries

	sources(entry: any)
		parseSources entry.data.sources ?? []

	// Editors (content.publish) hear about submissions for review.
	async notifyReview(entry: any, user: any)
		site := await settings(@c).get 'site'
		staff := await auth(@c).listUsers { kind: 'staff', status: 'active', limit: 500 }
		for u of staff
			continue if u.id === user?.id
			role := await auth(@c).role u.role
			continue unless role && (role.capabilities.includes('*') || role.capabilities.includes('content.*') || role.capabilities.includes('content.publish'))
			await mail(@c).send {
				to: u.email
				subject: `Review requested: ${entry.title}`
				template: 'review'
				text: `${user?.name || user?.email || 'An author'} submitted "${entry.title}" for review on ${site.title}.\n\nOpen it here:\n${siteOrigin()}/admin/content/${entry.type}/${entry.id}`
			}
		return
