import { page } from '@human-synthesis/norns/server'
import { fail } from '@sveltejs/kit'
import { apiKeys } from '$lib/norns/api/server/public'
import { auth, can, requireCap } from '$lib/norns/auth/server/public'
import { formToObject } from '$lib/norns/content/server/public'
import { siteOrigin } from '$lib/norns/settings/server/public'

// API keys. Holders of api.manage see and revoke every key; anyone can
// read the docs (linked from here).
export load := page.load
	handler: async ({ container, user }) =>
		manage := can user, 'api.manage'
		caps := auth(container).capabilities()
		grantable := manage ? apiKeys(container).grantable(user) : []
		groups := [] as any[]
		for cap of caps
			continue unless grantable.includes(cap.id)
			name := cap.group ?? 'Other'
			g .= groups.find((x) => x.name === name)
			unless g
				g = { name, caps: [] }
				groups.push g
			g.caps.push cap
		{
			manage
			keys: manage ? await apiKeys(container).list() : []
			groups
			full: can(user, '*') || (user.capabilities ?? []).includes('*')
			origin: siteOrigin()
		}

export actions := page.actions
	create:
		run: async ({ container, event, user }) =>
			requireCap user, 'api.manage'
			raw := formToObject await event.request.formData()
			caps := raw.full === 'on' ? ['*'] : (Array.isArray(raw.capabilities) ? raw.capabilities : (raw.capabilities ? [raw.capabilities] : []))
			r := await apiKeys(container).create { name: raw.name, capabilities: caps, expiresInDays: Number(raw.expires) || null }, user
			return fail(400, { errors: [{ path: [{ key: r.field ?? 'name' }], message: r.error }], values: raw }) unless r.ok
			{ created: { name: raw.name, key: r.key } }
	revoke:
		run: async ({ container, event, user }) =>
			requireCap user, 'api.manage'
			id := Number((await event.request.formData()).get('id'))
			r := await apiKeys(container).revoke id, user
			return fail(400, { error: r.error }) unless r.ok
			{ revoked: true }
