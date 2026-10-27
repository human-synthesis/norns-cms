import { page } from '@human-synthesis/norns/server'
import { error, fail } from '@sveltejs/kit'
import { settings } from '$lib/norns/settings/server/public'
import { cms, formToObject } from '$lib/norns/content/server/public'
import { requireCap, audit } from '$lib/norns/auth/server/public'
import { i18n } from '$lib/norns/i18n/server/public'

// One generic form for every registered settings group. A group may name
// its own capability (seo.manage, consent.manage); otherwise settings.manage.
groupOr404 := (container: any, id: string, user: any) =>
	group := settings(container).group id
	unless group
		throw error 404, 'Unknown settings group'
	requireCap user, group.capability ?? 'settings.manage'
	group

export load := page.load
	handler: async ({ container, params, user, url }) =>
		group := groupOr404 container, params.group, user
		translatable := settings(container).translatable group
		codes := cms(container).languages()
		def := cms(container).defaultLanguage()
		lang := codes.includes(url.searchParams.get('lang') ?? '') ? url.searchParams.get('lang') : def
		base := await settings(container).get group.id
		languages := codes.map (code) =>
			l := i18n(container).language code
			{ code, name: l.native || l.name || code }
		if lang !== def
			return {
				group: { id: group.id, label: group.label, description: group.description, fields: group.fields.filter((f) => f.translatable) }
				values: base._i18n?.[lang] ?? {}
				base
				lang, def, languages, translating: true
			}
		fields := []
		for f of group.fields
			key := f.type === 'reference' ? `entries:${f.to}` : (f.optionsFrom ?? null)
			fields.push key ? { ...f, options: await cms(container).optionsFor(key) } : f
		{ group: { id: group.id, label: group.label, description: group.description, fields }, values: base, base, lang, def, languages: translatable.length ? languages : [], translating: false }

export actions := page.actions
	save:
		run: async ({ container, event, user }) =>
			group := groupOr404 container, event.params.group, user
			raw := formToObject await event.request.formData()
			lang := String(event.url.searchParams.get('lang') ?? '')
			if lang && lang !== cms(container).defaultLanguage()
				await settings(container).saveTranslation group.id, lang, raw, user.id
				await audit(container).log user, 'settings.save', group.id, { language: lang }
				return { saved: true }
			r := await settings(container).save group.id, raw, user.id
			return fail(400, { errors: r.issues, values: raw }) unless r.ok
			await audit(container).log user, 'settings.save', group.id, { fields: Object.keys(raw).filter((k) => !k.startsWith('allow_')).length }
			{ saved: true }
