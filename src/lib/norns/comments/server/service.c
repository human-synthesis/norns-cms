import type { Container } from '@human-synthesis/norns/server'
import type { CommentsRepo } from './repo'
import { cms } from '../../content/server/public'
import { settings, siteOrigin } from '../../settings/server/public'
import { auth, can, isStaff } from '../../auth/server/public'
import { mail } from '../../mail/server/public'
import { tFor } from '../../i18n/server/public'
import { seo } from '../../seo/server/public'
import { commentHtml } from '../shared/settings'

EMAIL_RE := /^[^\s@]+@[^\s@]+\.[^\s@]+$/
DAY := 24 * 60 * 60 * 1000
MAX_BODY := 5000

live := (e: any) => e?.status === 'published' && (!e.published_at || e.published_at <= Date.now())
clamp := (n: any, lo: number, hi: number, d: number) => Math.min(hi, Math.max(lo, Number.isFinite(Number(n)) ? Math.round(Number(n)) : d))

hashIp := async (ip: string) =>
	return '' unless ip
	digest := await crypto.subtle.digest 'SHA-256', new TextEncoder().encode(`comments:${ip}`)
	[...new Uint8Array(digest)].slice(0, 12).map((b) => b.toString(16).padStart(2, '0')).join('')

export class CommentsService
	repo: CommentsRepo
	c: Container
	constructor(@repo: CommentsRepo, @c: Container)

	async config()
		s := await settings(@c).get 'comments'
		{
			...s
			types: (Array.isArray(s.types) ? s.types : []).filter((id: string) => cms(@c).getType(id)?.comments !== false && !!cms(@c).getType(id))
			maxDepth: clamp(s.maxDepth, 1, 5, 3)
			closeAfterDays: clamp(s.closeAfterDays, 0, 3650, 0)
			maxLinks: clamp(s.maxLinks, 0, 20, 2)
		}

	typeOptions()
		cms(@c).types().filter((ct) => ct.comments !== false).map((ct) => ({ value: ct.id, label: ct.labelPlural }))

	// The comment section of an entry: open | closed | off.
	async modeFor(ct: any, entry: any, cfg: any = null)
		cfg ??= await @config()
		return 'off' unless cfg.enabled && ct && ct.comments !== false && entry
		chosen := String(entry.data?.commentsMode ?? '')
		return 'off' if chosen === 'off'
		mode := chosen || (cfg.types.includes(ct.id) ? 'open' : 'off')
		return mode unless mode === 'open'
		published := Number(entry.published_at || entry.created_at || 0)
		return 'closed' if cfg.closeAfterDays > 0 && published && Date.now() - published > cfg.closeAfterDays * DAY
		'open'

	// Rows → reading order (each thread after its parent), with depth.
	// Replies whose parent is not shown start their own thread.
	thread(rows: any[])
		ids := new Set rows.map((r) => r.id)
		children := new Map<number, any[]>()
		roots := [] as any[]
		for r of rows
			if r.parent_id && ids.has(r.parent_id)
				children.set r.parent_id, [...(children.get(r.parent_id) ?? []), r]
			else
				roots.push r
		out := [] as any[]
		visit := (r: any, depth: number) =>
			out.push { ...r, depth }
			for child of (children.get(r.id) ?? [])
				visit child, depth + 1
			return
		for r of roots
			visit r, 0
		out

	// What the theme shows under an entry: approved comments (plus the
	// viewer's own waiting ones) and whether / how they can write.
	async section(ct: any, entry: any, viewer: { user?: any } = {})
		cfg := await @config()
		mode := await @modeFor ct, entry, cfg
		return null if mode === 'off'
		user := viewer.user ?? null
		rows := (await @repo.forEntry entry.id, ['approved', 'pending']).filter (r: any) =>
			r.status === 'approved' || (user && r.user_id === user.id)
		comments := @thread(rows).map (r: any) => ({
			id: r.id
			parentId: r.parent_id
			depth: r.depth
			name: r.author_name
			staff: !!r.staff
			html: commentHtml(r.body)
			createdAt: r.created_at
			pending: r.status !== 'approved'
			canReply: mode === 'open' && cfg.replies && r.status === 'approved' && r.depth < cfg.maxDepth
		})
		{
			entryId: entry.id
			mode
			count: rows.filter((r: any) => r.status === 'approved').length
			comments
			canPost: mode === 'open' && (cfg.who === 'anyone' || !!user)
			signIn: mode === 'open' && cfg.who === 'members' && !user
			guest: !user
			replies: !!cfg.replies
		}

	// Approved comments for structured data (newest last, at most `limit`).
	async approved(entryId: number, limit = 20)
		rows := await @repo.forEntry entryId, ['approved']
		{ count: rows.length, comments: rows.slice(0, limit).map((r: any) => ({ name: r.author_name, text: r.body, createdAt: r.created_at, staff: !!r.staff })) }

	// A new comment. input: { entryId, parentId?, body, name?, email? };
	// ctx: { user, ip, lang, spam (protection verdict), viaApi }.
	// → { ok, id, status, message } | { ok: false, status, error?, errors? }
	async post(input: Record<string, any>, ctx: Record<string, any> = {})
		t := await tFor @c, ctx.lang || 'en'
		cfg := await @config()
		user := ctx.user ?? null
		// Through the API the user carries only the key's capabilities: a
		// staff account's key writes as the team.
		staff := isStaff(user) || (!!ctx.viaApi && user?.kind === 'staff')
		entry := await cms(@c).byId Number(input.entryId)
		ct := entry ? cms(@c).getType(entry.type) : null
		readable := entry && live(entry) && (entry.visibility !== 'members' || staff || can(user, 'content.members'))
		return { ok: false, status: 404, error: t('comments.unavailable') } unless ct && readable
		mode := await @modeFor ct, entry, cfg
		// The team may still answer in a closed section.
		return { ok: false, status: 403, error: t('comments.closed') } unless mode === 'open' || (staff && mode === 'closed')
		return { ok: false, status: 403, error: t('comments.signIn') } if cfg.who === 'members' && !user
		unless staff || ctx.viaApi
			return { ok: false, status: 429, error: t('comments.rate') } unless await auth(@c).rateLimit('comment', ctx.ip || 'unknown', 5, 10 * 60 * 1000)
		errors := {} as Record<string, string>
		body := String(input.body ?? '').replace(/\r\n?/g, '\n').trim().slice(0, MAX_BODY)
		errors.body = t('comments.required') if body.length < 2
		name .= String(user?.name || input.name || '').trim().slice(0, 80)
		email .= String(user?.email || input.email || '').trim().split(/\s/)[0].toLowerCase().slice(0, 200)
		unless user
			errors.name = t('comments.nameRequired') unless name
			errors.email = t('form.invalidEmail') unless EMAIL_RE.test(email)
		name ||= user ? (user.email ?? '').split('@')[0] : ''
		parent .= null as any
		if input.parentId
			parent = await @repo.get Number(input.parentId)
			if !parent || parent.entry_id !== entry.id || parent.status !== 'approved'
				errors.body ??= t('comments.unavailable')
			else if !cfg.replies || parent.depth >= cfg.maxDepth
				errors.body ??= t('comments.tooDeep')
		return { ok: false, status: 400, errors } if Object.keys(errors).length
		links := (body.match(/https?:\/\//g) ?? []).length
		spam := !staff && (!!ctx.spam || links > cfg.maxLinks)
		status := if spam
			'spam'
		else if staff || cfg.moderation === 'none' || (cfg.moderation === 'guests' && user)
			'approved'
		else
			'pending'
		id := await @repo.insert {
			entryId: entry.id, parentId: parent?.id ?? null, depth: (parent ? parent.depth + 1 : 0)
			userId: user?.id ?? null, staff, name, email, body, status
			locale: entry.locale ?? '', ipHash: await hashIp(String(ctx.ip ?? '')), approvedBy: (staff ? user.id : null)
		}
		await @notify(ct, entry, id, name, body, status) if cfg.notify && !staff && status !== 'spam'
		await seo(@c).commentApproved(ct, entry) if status === 'approved'
		{ ok: true, id, status, message: (status === 'pending' ? t('comments.pending') : t('comments.posted')) }

	async notify(ct: any, entry: any, id: number, name: string, body: string, status: string)
		try
			recipients := await mail(@c).notifyTo()
			return unless recipients.length
			site := await settings(@c).get 'site'
			subject := `${status === 'pending' ? 'Comment waiting for approval' : 'New comment'} on “${entry.title}” — ${site.title}`
			text := [`${name} wrote on ${cms(@c).absoluteUrlFor(ct, entry)}:`, '', body, '', `Moderate: ${siteOrigin()}/admin/comments?status=${status}#comment-${id}`].join('\n')
			for to of recipients
				await mail(@c).send { purpose: 'notifications', to, subject, text, template: 'comment' }
		catch e
			console.error 'comments.notify', e
		return

	// --- moderation ------------------------------------------------------------

	async get(id: number)
		@repo.get id

	async list(opts: Record<string, any> = {})
		perPage := clamp(opts.perPage, 1, 200, 50)
		pageNo := clamp(opts.page, 1, 100000, 1)
		q := { status: opts.status || undefined, entryId: Number(opts.entryId) || undefined, q: opts.q || undefined }
		rows := await @repo.list { ...q, limit: perPage, offset: (pageNo - 1) * perPage }
		entries := {} as Record<number, any>
		for id of [...new Set(rows.map((r: any) => r.entry_id))]
			e := await cms(@c).byId id
			ct := e ? cms(@c).getType(e.type) : null
			entries[id] = e && ct ? { id: e.id, type: e.type, title: e.title, url: cms(@c).absoluteUrlFor(ct, e) } : { id, type: null, title: '(deleted entry)', url: null }
		{
			comments: rows.map (r: any) => ({ ...r, html: commentHtml(r.body), entry: entries[r.entry_id] })
			total: await @repo.count(q)
			page: pageNo
			perPage
			counts: await @repo.counts()
		}

	async setStatus(id: number, status: string, user: any = null)
		row := await @repo.get id
		return null unless row
		await @repo.setStatus id, status, user?.id ?? null
		if status === 'approved' && row.status !== 'approved'
			entry := await cms(@c).byId row.entry_id
			ct := entry ? cms(@c).getType(entry.type) : null
			await seo(@c).commentApproved(ct, entry) if ct && live(entry)
		@repo.get id

	async edit(id: number, body: string)
		row := await @repo.get id
		return null unless row
		text := String(body ?? '').replace(/\r\n?/g, '\n').trim().slice(0, MAX_BODY)
		return null unless text
		await @repo.setBody id, text
		@repo.get id

	async remove(id: number)
		@repo.remove id

	// The team answers from the admin (or the API): approved at once.
	async reply(id: number, body: string, user: any)
		parent := await @repo.get id
		return { ok: false, status: 404, error: 'Comment not found' } unless parent
		await @setStatus(id, 'approved', user) unless parent.status === 'approved'
		cfg := await @config()
		r := await @post { entryId: parent.entry_id, parentId: (parent.depth < cfg.maxDepth ? id : parent.parent_id), body }, { user, viaApi: true, lang: parent.locale || 'en' }
		r.ok ? r : { ...r, error: r.error ?? Object.values(r.errors ?? {})[0] }

	counts()
		@repo.counts()

	pending()
		@repo.count { status: 'pending' }

	entryRemoved(entryId: number)
		@repo.removeForEntry entryId

	byUser(userId: number)
		@repo.byUser userId

	// Daily: spam and trash older than 30 days go; comments of deleted
	// members lose their name and email.
	async purge()
		removed := await @repo.purge Date.now() - 30 * DAY
		anonymized .= 0
		for id of await @repo.userIds()
			u := await auth(@c).getUser id
			if !u || u.status === 'deleted'
				await @repo.anonymize id, 'Former member'
				anonymized++
		{ removed, anonymized }
