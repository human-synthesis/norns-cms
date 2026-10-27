import { page } from '@human-synthesis/norns/server'
import { error, fail, redirect } from '@sveltejs/kit'
import { i18n } from '$lib/norns/i18n/server/public'
import { requireCap, audit } from '$lib/norns/auth/server/public'
import { formToObject } from '$lib/norns/content/server/public'

export load := page.load
	handler: async ({ container, params, user, url }) =>
		requireCap user, 'i18n.manage'
		isNew := params.id === 'new'
		m := isNew ? null : await i18n(container).market(Number(params.id))
		if !isNew && !m
			throw error 404, 'Localisation not found'
		{ m, languages: await i18n(container).allLanguages(), saved: url.searchParams.has('saved'), primaryHost: (await i18n(container).config()).primaryHost }

// Form → saveMarket input: languages are the ticked codes with their
// segments (`seg_<code>`).
fromForm := (raw: any) =>
	ticked := Array.isArray(raw.languages) ? raw.languages : (raw.languages ? [raw.languages] : [])
	{
		...raw
		languages: ticked.map((code: string) => ({ code, segment: raw[`seg_${code}`] ?? '' }))
	}

export actions := page.actions
	save:
		run: async ({ container, event, user }) =>
			requireCap user, 'i18n.manage'
			raw := formToObject await event.request.formData()
			id := event.params.id === 'new' ? null : Number(event.params.id)
			r := await i18n(container).saveMarket id, fromForm(raw)
			return fail(400, { errors: [{ path: [{ key: r.field ?? 'code' }], message: r.error }], values: raw }) unless r.ok
			await audit(container).log user, 'i18n.localisation_save', String(raw.code), {}
			throw redirect 303, `/admin/localisation/${r.id}?saved=1`
	remove:
		run: async ({ container, event, user }) =>
			requireCap user, 'i18n.manage'
			r := await i18n(container).deleteMarket Number(event.params.id)
			return fail(400, { error: r.error }) unless r.ok
			await audit(container).log user, 'i18n.localisation_delete', `#${event.params.id}`, {}
			throw redirect 303, '/admin/localisation'
