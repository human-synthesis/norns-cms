import { page } from '@human-synthesis/norns/server'
import { error, fail } from '@sveltejs/kit'
import { settings } from '$lib/norns/settings/server/public'
import { cms, formToObject } from '$lib/norns/content/server/public'
import { requireCap, audit } from '$lib/norns/auth/server/public'

// One generic form for every registered settings group. A group may name
// its own capability (seo.manage, consent.manage); otherwise settings.manage.
groupOr404 := (container: any, id: string, user: any) =>
	group := settings(container).group id
	unless group
		throw error 404, 'Unknown settings group'
	requireCap user, group.capability ?? 'settings.manage'
	group

export load := page.load
	handler: async ({ container, params, user }) =>
		group := groupOr404 container, params.group, user
		fields := []
		for f of group.fields
			key := f.type === 'reference' ? `entries:${f.to}` : (f.optionsFrom ?? null)
			fields.push key ? { ...f, options: await cms(container).optionsFor(key) } : f
		{ group: { id: group.id, label: group.label, description: group.description, fields }, values: await settings(container).get(group.id) }

export actions := page.actions
	save:
		run: async ({ container, event, user }) =>
			group := groupOr404 container, event.params.group, user
			raw := formToObject await event.request.formData()
			r := await settings(container).save group.id, raw, user.id
			return fail(400, { errors: r.issues, values: raw }) unless r.ok
			await audit(container).log user, 'settings.save', group.id, { fields: Object.keys(raw).filter((k) => !k.startsWith('allow_')).length }
			{ saved: true }
