import { page } from '@human-synthesis/norns/server'
import { error, fail, redirect } from '@sveltejs/kit'
import { i18n } from '$lib/norns/i18n/server/public'
import { requireCap, audit } from '$lib/norns/auth/server/public'

export load := page.load
	handler: async ({ container, params, user, url }) =>
		requireCap user, 'i18n.manage'
		languages := await i18n(container).allLanguages()
		l := languages.find((x) => x.code === params.code)
		unless l
			throw error 404, 'Language not found'
		{ l, others: languages.filter((x) => x.code !== l.code), added: url.searchParams.has('added'), saved: url.searchParams.has('saved') }

export actions := page.actions
	save:
		run: async ({ container, event, user }) =>
			requireCap user, 'i18n.manage'
			fd := await event.request.formData()
			v := Object.fromEntries(fd) as Record<string, string>
			r := await i18n(container).saveLanguage {
				code: event.params.code
				name: v.name, native: v.native, dir: v.dir, fallback: v.fallback, sort: v.sort
				enabled: v.enabled !== 'false'
				paths: { article: v.path_article, authors: v.path_authors, category: v.path_category, tag: v.path_tag }
			}
			return fail(400, { error: r.error }) unless r.ok
			await audit(container).log user, 'i18n.language_save', event.params.code, {}
			throw redirect 303, `/admin/localisation/languages/${encodeURIComponent(event.params.code)}?saved=1`
	remove:
		run: async ({ container, event, user }) =>
			requireCap user, 'i18n.manage'
			r := await i18n(container).deleteLanguage event.params.code
			return fail(400, { error: r.error }) unless r.ok
			await audit(container).log user, 'i18n.language_delete', event.params.code, {}
			throw redirect 303, '/admin/localisation/languages'
