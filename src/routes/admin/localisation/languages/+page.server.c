import { page } from '@human-synthesis/norns/server'
import { fail, redirect } from '@sveltejs/kit'
import { i18n } from '$lib/norns/i18n/server/public'
import { requireCap, audit } from '$lib/norns/auth/server/public'

export load := page.load
	handler: async ({ container, user }) =>
		requireCap user, 'i18n.manage'
		languages := await i18n(container).allLanguages()
		{ languages, known: i18n(container).known().filter((k) => !languages.some((l) => l.code === k.code)) }

export actions := page.actions
	add:
		run: async ({ container, event, user }) =>
			requireCap user, 'i18n.manage'
			fd := await event.request.formData()
			code := String(fd.get('custom') || fd.get('code') || '').trim()
			r := await i18n(container).saveLanguage { code, fallback: code.includes('-') ? code.split('-')[0] : '' }
			return fail(400, { error: r.error }) unless r.ok
			await audit(container).log user, 'i18n.language_save', code, {}
			throw redirect 303, `/admin/localisation/languages/${encodeURIComponent(code)}?added=1`
