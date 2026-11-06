import type { Container } from '@human-synthesis/norns/server'
import type { ProtectionRepo } from './repo'
import { DEFAULT_FILTER, cleanFilter, allowed, countryName, countryOptions, PRESETS } from '../shared/countries.js'

// Country filter: only visitors from allowed countries (or: all but the
// blocked ones) reach the site; the rest get an empty 403 from
// blockHandle, before anything else runs. The team's pages (admin,
// sign-in, API) are left out unless `admin` is on — and then saving is
// refused if it would shut out the person saving. The country is
// Cloudflare's (request.cf.country); without it (dev, tests) it is
// unknown and `unknown` decides.

TTL := 60 * 1000
FLUSH_EVERY := 60 * 1000

cache := new Map<any, { at: number, filter: any }>()
pending := new Map<any, { hits: Map<string, { n: number, at: number }>, flushedAt: number }>()

export countryOf := (event: any) =>
	String(event?.request?.cf?.country ?? event?.platform?.cf?.country ?? event?.request?.headers?.get?.('cf-ipcountry') ?? '').toUpperCase()

export class CountryFilter
	repo: ProtectionRepo
	c: Container
	constructor(@repo: ProtectionRepo, @c: Container)

	key()
		db := (@repo as any).db
		db?.raw ?? db

	async filter()
		hit := cache.get @key()
		return hit.filter if hit && Date.now() - hit.at < TTL
		filter .= DEFAULT_FILTER as any
		try
			raw := await @repo.state 'country_filter'
			filter = cleanFilter(JSON.parse(raw)) if raw
		catch
			filter = DEFAULT_FILTER
		cache.set @key(), { at: Date.now(), filter }
		filter

	// → null when the request may pass, else the country it was refused for.
	async refuse(event: any)
		f := await @filter()
		return null if f.mode === 'off'
		country := countryOf event
		allowed(f, country, event.url.pathname) ? null : (country || 'XX')

	// input: { mode, countries, unknown, admin }; ctx.country: where the
	// person saving is (their request).
	async save(input: any, ctx: { user?: any, country?: string } = {})
		f := cleanFilter input
		return { ok: false, error: 'Pick at least one country to allow' } if f.mode === 'allow' && !f.countries.length
		return { ok: false, error: 'Pick at least one country to block' } if f.mode === 'block' && !f.countries.length
		if f.admin && !allowed(f, ctx.country ?? '', '/admin')
			where := ctx.country ? `${countryName(ctx.country)} (${ctx.country})` : 'an unknown country'
			return { ok: false, error: `You are connecting from ${where}: with these settings you would lock yourself out of the admin. Allow it, or leave the admin out of the filter.` }
		await @repo.setState 'country_filter', JSON.stringify(f), ctx.user?.id ?? null
		cache.delete @key()
		await (@c.resolve('protection.cloudflare') as any).changed()
		{ ok: true, filter: f }

	hit(country: string, waitUntil: ((p: Promise<any>) => void) | null = null)
		state := pending.get(@key()) ?? { hits: new Map(), flushedAt: Date.now() }
		pending.set @key(), state
		cur := state.hits.get(country) ?? { n: 0, at: 0 }
		state.hits.set country, { n: cur.n + 1, at: Date.now() }
		if Date.now() - state.flushedAt >= FLUSH_EVERY
			job := @flushHits()
			waitUntil(job.catch(=> null)) if waitUntil
		return

	async flushHits()
		state := pending.get @key()
		return 0 unless state && state.hits.size
		entries := [...state.hits.entries()]
		state.hits = new Map()
		state.flushedAt = Date.now()
		for [country, h] of entries
			await @repo.countCountryBlocks country, h.n, h.at
		entries.length

	async stats()
		await @flushHits()
		(await @repo.countryBlocks()).map (r: any) => ({ ...r, name: (r.country === 'XX' ? 'Unknown' : countryName(r.country)) })

	clearStats()
		@repo.clearCountryBlocks()

	async status(event: any = null)
		country := event ? countryOf(event) : ''
		{
			filter: await @filter()
			options: countryOptions()
			presets: PRESETS
			you: { country, name: (country ? countryName(country) : 'Unknown (no Cloudflare country on this request)') }
			stats: await @stats()
		}
