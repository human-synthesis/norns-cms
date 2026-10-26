import { page } from '@human-synthesis/norns/server'
import { error, fail, redirect } from '@sveltejs/kit'
import { auth, requireCap } from '$lib/norns/auth/server/public'
import { formToObject } from '$lib/norns/content/server/public'

export load := page.load
	handler: async ({ container, params, user }) =>
		requireCap user, 'roles.manage'
		isNew := params.id === 'new'
		role := isNew ? null : await auth(container).role(params.id)
		if !isNew && !role
			throw error 404, 'Role not found'
		caps := auth(container).capabilities()
		groups := [] as any[]
		for cap of caps
			name := cap.group ?? 'Other'
			g .= groups.find((x) => x.name === name)
			unless g
				g = { name, caps: [] }
				groups.push g
			g.caps.push cap
		{ role, groups, isNew }

export actions := page.actions
	save:
		run: async ({ container, event, user }) =>
			requireCap user, 'roles.manage'
			raw := formToObject await event.request.formData()
			caps := Array.isArray(raw.capabilities) ? raw.capabilities : (raw.capabilities ? [raw.capabilities] : [])
			id := event.params.id === 'new' ? undefined : event.params.id
			r := await auth(container).saveRole { id, kind: raw.kind, label: raw.label, description: raw.description, capabilities: caps }, user
			return fail(400, { errors: [{ path: [{ key: r.field ?? 'label' }], message: r.error }], values: raw, error: r.field ? undefined : r.error }) unless r.ok
			throw redirect 303, `/admin/roles/${r.id}?saved=1`
	remove:
		run: async ({ container, event, user }) =>
			requireCap user, 'roles.manage'
			r := await auth(container).deleteRole event.params.id, user
			return fail(400, { error: r.error }) unless r.ok
			throw redirect 303, '/admin/roles'
