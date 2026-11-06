import type { Container } from '@human-synthesis/norns/server'
import type { ProtectionRepo } from './repo'
import { envVar, siteOrigin, sealValue, openValue, currentEvent, backgroundTask } from '../../settings/server/public'
import { i18n } from '../../i18n/server/public'
import { pathRules, countryRule, isOurs, zoneCandidates } from '../shared/firewall.js'
import { cleanFilter, DEFAULT_FILTER } from '../shared/countries.js'

// Cloudflare firewall: the blocked paths and the country filter mirrored
// as WAF custom rules on the site's zone (Rulesets API, phase
// http_request_firewall_custom), so Cloudflare refuses those requests at
// the edge before they reach the Worker. Only rules the CMS made (ref
// norns_cms_*) are created, changed or removed; each is limited to the
// site's own hostnames. The API token (Zone WAF: Edit, Zone: Read) is
// sealed with CMS_SECRET; CLOUDFLARE_API_TOKEN / CLOUDFLARE_ZONE_ID in the
// environment win. The Worker keeps filtering too.

API := 'https://api.cloudflare.com/client/v4'
PHASE := 'http_request_firewall_custom'
PURPOSE := 'cloudflare-token'
KEY := 'cloudflare'

class CfError extends Error
	status: number
	constructor(message: string, @status: number = 0)
		super message

export class CloudflareFirewall
	repo: ProtectionRepo
	c: Container
	constructor(@repo: ProtectionRepo, @c: Container)

	async stored()
		try
			JSON.parse((await @repo.state(KEY)) ?? '{}') ?? {}
		catch
			{}

	async write(values: Record<string, any>, userId: number | null = null)
		await @repo.setState KEY, JSON.stringify({ ...(await @stored()), ...values }), userId
		return

	async token()
		envVar('CLOUDFLARE_API_TOKEN') || openValue((await @stored()).token, PURPOSE) || ''

	// The site's hostnames (every localisation) on a zone.
	hosts(zone: string)
		all := new Set<string>()
		all.add new URL(siteOrigin()).hostname
		for code of i18n(@c).contentLanguages()
			for place of i18n(@c).placesOf(code)
				try
					all.add new URL(place.origin || siteOrigin()).hostname
				catch
					continue
		[...all].filter((h) => zone && (h === zone || h.endsWith(`.${zone}`)))

	async call(path: string, init: Record<string, any> = {})
		token := await @token()
		throw new CfError('No Cloudflare API token saved') unless token
		res := await fetch `${API}${path}`, { ...init, headers: { authorization: `Bearer ${token}`, 'content-type': 'application/json', ...(init.headers ?? {}) } }
		body := await res.json().catch(=> ({})) as any
		unless res.ok && body.success !== false
			message := body.errors?.map((e: any) => e.message).filter(Boolean).join('; ') || `HTTP ${res.status}`
			throw new CfError(message, res.status)
		body.result

	// The zone: the configured id, else found from the site's host.
	async zone()
		s := await @stored()
		id := envVar('CLOUDFLARE_ZONE_ID') || s.zoneId
		if id
			z := await @call `/zones/${encodeURIComponent(id)}`
			return { id: z.id, name: z.name }
		host := new URL(siteOrigin()).hostname
		for name of zoneCandidates(host)
			found := await @call `/zones?name=${encodeURIComponent(name)}`
			return { id: found[0].id, name: found[0].name } if found?.length
		throw new CfError(`No zone for ${host} is visible to this token`)

	async status()
		s := await @stored()
		{
			tokenSaved: !!s.token
			tokenFromEnv: !!envVar('CLOUDFLARE_API_TOKEN')
			zoneFromEnv: !!envVar('CLOUDFLARE_ZONE_ID')
			canSeal: !!envVar('CMS_SECRET')
			zoneId: s.zoneId ?? ''
			zoneName: s.zoneName ?? ''
			paths: s.paths ?? true
			countries: s.countries ?? true
			auto: s.auto ?? true
			lastSyncAt: s.lastSyncAt ?? null
			lastError: s.lastError ?? null
			rules: s.rules ?? []
			ready: !!(s.token || envVar('CLOUDFLARE_API_TOKEN'))
		}

	// input: { token?, clearToken?, zoneId?, paths?, countries?, auto? };
	// a new token or zone is checked with Cloudflare before it is kept.
	async save(input: Record<string, any>, user: any = null)
		s := await @stored()
		next := { ...s } as Record<string, any>
		token := String(input.token ?? '').trim()
		if token
			return { ok: false, error: 'Set the CMS_SECRET secret first: the token is stored encrypted with it' } unless envVar('CMS_SECRET')
			next.token = sealValue token, PURPOSE
		next.token = null if input.clearToken
		next.zoneId = String(input.zoneId ?? '').trim() if input.zoneId !== undefined
		for k of ['paths', 'countries', 'auto']
			next[k] = !!input[k] if input[k] !== undefined
		previous := s
		await @write next, user?.id ?? null
		if (token || input.zoneId !== undefined) && await @token()
			try
				await @call '/user/tokens/verify'
				z := await @zone()
				await @write { zoneId: z.id, zoneName: z.name }
			catch e
				await @repo.setState KEY, JSON.stringify(previous), user?.id ?? null
				return { ok: false, error: `Cloudflare: ${(e as any).message}` }
		// With a token, the rules follow the saved switches at once.
		synced := (await @token()) ? await @sync() : null
		{ ok: true, synced, status: await @status() }

	// The rules the CMS wants on the zone right now.
	async desired(zoneName: string)
		s := await @status()
		hosts := @hosts zoneName
		out := [] as any[]
		if s.paths
			patterns := (await @repo.blockedPaths()).filter((r: any) => r.enabled).map((r: any) => r.pattern)
			out.push ...pathRules(patterns, hosts)
		if s.countries
			filter .= DEFAULT_FILTER as any
			try
				raw := await @repo.state 'country_filter'
				filter = cleanFilter(JSON.parse(raw)) if raw
			catch
				filter = DEFAULT_FILTER
			rule := countryRule filter, hosts
			out.push rule if rule
		{ rules: out, hosts }

	// Bring the zone's CMS rules in line: create, update, delete. Other
	// rules on the zone are left as they are.
	async sync(opts: { remove?: boolean } = {})
		try
			z := await @zone()
			{ rules, hosts } := opts.remove ? { rules: [], hosts: [] } : await @desired(z.name)
			throw new CfError(`None of this site's hostnames is on the zone ${z.name}`) if !opts.remove && !@hosts(z.name).length
			body := (r: any) => ({ action: 'block', expression: r.expression, description: r.description, ref: r.ref, enabled: true })
			ruleset .= null as any
			try
				ruleset = await @call `/zones/${z.id}/rulesets/phases/${PHASE}/entrypoint`
			catch e
				throw e unless (e as any).status === 404
			if !ruleset
				await @call(`/zones/${z.id}/rulesets/phases/${PHASE}/entrypoint`, { method: 'PUT', body: JSON.stringify({ rules: rules.map(body) }) }) if rules.length
			else
				ours := (ruleset.rules ?? []).filter(isOurs)
				for r of rules
					existing := ours.find((x: any) => x.ref === r.ref)
					if existing
						await @call `/zones/${z.id}/rulesets/${ruleset.id}/rules/${existing.id}`, { method: 'PATCH', body: JSON.stringify(body(r)) }
					else
						await @call `/zones/${z.id}/rulesets/${ruleset.id}/rules`, { method: 'POST', body: JSON.stringify(body(r)) }
				for x of ours
					continue if rules.some((r) => r.ref === x.ref)
					await @call `/zones/${z.id}/rulesets/${ruleset.id}/rules/${x.id}`, { method: 'DELETE' }
			summary := rules.map((r) => ({ ref: r.ref, description: r.description, length: r.expression.length }))
			await @write { lastSyncAt: Date.now(), lastError: null, rules: summary, zoneId: z.id, zoneName: z.name }
			{ ok: true, zone: z.name, hosts, rules: summary }
		catch e
			message := String((e as any)?.message ?? e).slice(0, 500)
			await @write { lastError: message, lastErrorAt: Date.now() }
			{ ok: false, error: message }

	// After a change to the blocked paths or the country filter: sync when
	// automatic sync is on (after the response when possible).
	async changed()
		s := await @status()
		return unless s.ready && s.auto
		later := backgroundTask()
		if later
			later @sync().catch(=> null)
		else
			await @sync()
		return
