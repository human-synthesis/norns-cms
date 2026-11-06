import type { Container } from '@human-synthesis/norns/server'
import type { ProtectionRepo } from './repo'
import { DEFAULT_PATTERNS, normalizePattern, patternRegex, patternProblem, looksLikeProbe, suggestPattern } from '../shared/paths.js'

// Blocked paths: requests for paths scanners probe (/wp-login.php, /.env,
// *.php …) are answered with an empty 403 by blockHandle, first in the
// chain, so no session, database query (the list is cached per isolate),
// rendering or 404 logging happens for them. A Worker cannot drop the TCP
// connection itself; that needs a Cloudflare WAF rule.

TTL := 60 * 1000
FLUSH_EVERY := 60 * 1000

// Per isolate and database: compiled rules, and hits not yet written.
cache := new Map<any, { at: number, rules: { id: number, pattern: string, re: RegExp }[] }>()
pending := new Map<any, { hits: Map<number, { n: number, at: number }>, flushedAt: number }>()

export class BlockedPaths
	repo: ProtectionRepo
	c: Container
	constructor(@repo: ProtectionRepo, @c: Container)

	key()
		db := (@repo as any).db
		db?.raw ?? db

	async rules()
		hit := cache.get @key()
		return hit.rules if hit && Date.now() - hit.at < TTL
		rows := (await @repo.blockedPaths()).filter((r: any) => r.enabled)
		rules := rows.map (r: any) => ({ id: r.id, pattern: r.pattern, re: patternRegex(r.pattern) })
		cache.set @key(), { at: Date.now(), rules }
		rules

	invalidate()
		cache.delete @key()
		return

	firewall()
		@c.resolve('protection.cloudflare') as any

	// The rule a path falls under, or null.
	async match(path: string)
		p := String(path ?? '').toLowerCase()
		decoded .= p
		try
			decoded = decodeURIComponent(p)
		catch
			decoded = p
		for r of await @rules()
			return r if r.re.test(p) || r.re.test(decoded)
		null

	// Counted in memory; written at most once a minute per isolate.
	hit(ruleId: number, waitUntil: ((p: Promise<any>) => void) | null = null)
		state := pending.get(@key()) ?? { hits: new Map(), flushedAt: Date.now() }
		pending.set @key(), state
		cur := state.hits.get(ruleId) ?? { n: 0, at: 0 }
		state.hits.set ruleId, { n: cur.n + 1, at: Date.now() }
		if Date.now() - state.flushedAt >= FLUSH_EVERY
			job := @flushHits()
			if waitUntil
				waitUntil job.catch(=> null)
		return

	async flushHits()
		state := pending.get @key()
		return 0 unless state && state.hits.size
		entries := [...state.hits.entries()]
		state.hits = new Map()
		state.flushedAt = Date.now()
		for [id, h] of entries
			await @repo.countBlockedHits id, h.n, h.at
		entries.length

	async list()
		await @flushHits()
		@repo.blockedPaths()

	// → { ok: true, pattern, existed } | { ok: false, error }
	async add(input: { pattern: string, note?: string, source?: string }, user: any = null)
		pattern := normalizePattern input.pattern
		problem := patternProblem pattern
		return { ok: false, error: problem } if problem
		existing := await @repo.blockedByPattern pattern
		if existing
			await @repo.updateBlockedPath(existing.id, { enabled: true }) unless existing.enabled
		else
			source := ['manual', '404', 'default'].includes(input.source ?? '') ? input.source : 'manual'
			await @repo.addBlockedPath pattern, String(input.note ?? '').trim().slice(0, 200), source, user?.id ?? null
		@invalidate()
		await @firewall().changed()
		{ ok: true, pattern, existed: !!existing, row: await @repo.blockedByPattern(pattern) }

	async update(id: number, fields: { enabled?: boolean, note?: string })
		row := await @repo.blockedPath id
		return null unless row
		await @repo.updateBlockedPath id, { enabled: (fields.enabled === undefined ? undefined : !!fields.enabled), note: (fields.note === undefined ? undefined : String(fields.note).trim().slice(0, 200)) }
		@invalidate()
		await @firewall().changed() if fields.enabled !== undefined
		@repo.blockedPath id

	async remove(id: number)
		await @repo.removeBlockedPath id
		@invalidate()
		await @firewall().changed()
		return

	async restoreDefaults()
		n .= 0
		for [pattern, note] of DEFAULT_PATTERNS
			unless await @repo.blockedByPattern(pattern)
				await @repo.addBlockedPath pattern, note, 'default', null
				n++
		@invalidate()
		await @firewall().changed() if n
		n

	// Check a path: which rule blocks it, or why it cannot be blocked.
	async check(path: string)
		rule := await @match normalizePattern(path)
		{ path: normalizePattern(path), blocked: !!rule, pattern: rule?.pattern ?? null, suggestion: suggestPattern(path), probe: looksLikeProbe(path) }

	// 404 log rows (seo notFound) → those that look like scans, each with
	// a suggested pattern, grouped by pattern.
	suggestions(notFound: any[])
		groups := new Map<string, { pattern: string, paths: string[], hits: number, problem: string }>()
		for row of notFound
			continue unless looksLikeProbe(row.path)
			pattern := suggestPattern row.path
			g := groups.get(pattern) ?? { pattern, paths: [], hits: 0, problem: patternProblem(pattern) }
			g.paths.push row.path
			g.hits += Number(row.hits ?? 0)
			groups.set pattern, g
		[...groups.values()].filter((g) => !g.problem).sort((a, b) => b.hits - a.hits)
