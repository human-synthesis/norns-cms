import { page } from '@human-synthesis/norns/server'
import { i18n, aiAvailable, aiTranslateTexts } from '$lib/norns/i18n/server/public'
import { fail } from '@sveltejs/kit'
import { requireCap, audit } from '$lib/norns/auth/server/public'

export load := page.load
	handler: async ({ container, url, user }) =>
		requireCap user, 'i18n.manage'
		languages := await i18n(container).allLanguages()
		lang := url.searchParams.get('lang') ?? languages.find((l) => l.code !== 'en')?.code ?? languages[0]?.code ?? 'en'
		rows := await i18n(container).stringTable lang
		{ languages, lang, rows, missing: url.searchParams.get('missing') === '1', q: url.searchParams.get('q') ?? '', ai: aiAvailable() }

export actions := page.actions
	save:
		run: async ({ container, event, user }) =>
			requireCap user, 'i18n.manage'
			fd := await event.request.formData()
			lang := String(fd.get('lang') ?? '')
			values := {} as Record<string, string>
			for [k, v] of fd.entries()
				values[k.slice(2)] = String(v) if k.startsWith('k:')
			n := await i18n(container).setStrings lang, values
			await audit(container).log user, 'i18n.texts_save', lang, { count: n }
			{ saved: n }
	ai:
		run: async ({ container, event, user }) =>
			requireCap user, 'i18n.manage'
			lang := String((await event.request.formData()).get('lang') ?? '')
			try
				n := await aiTranslateTexts container, lang
				await audit(container).log user, 'i18n.texts_ai', lang, { count: n }
				return { saved: n, ai: n }
			catch e
				return fail 400, { error: `AI translation failed: ${e.message}` }
