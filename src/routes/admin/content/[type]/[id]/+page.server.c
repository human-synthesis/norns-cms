import { page } from '@human-synthesis/norns/server'
import { error, fail, redirect } from '@sveltejs/kit'
import { cms, canEditEntry, formToObject, typeOrRedirect } from '$lib/norns/content/server/public'
import { requireCap, can, auth, audit } from '$lib/norns/auth/server/public'
import { settings } from '$lib/norns/settings/server/public'
import { i18n, aiAvailable, aiTranslateData } from '$lib/norns/i18n/server/public'

typeOr404 := (container: any, id: string) =>
	ct := cms(container).getType id
	unless ct
		throw error 404, `Unknown content type "${id}"`
	ct

entryOr404 := async (container: any, ct: any, idParam: string) =>
	return null if idParam === 'new'
	entry := await cms(container).get ct.id, Number(idParam)
	unless entry
		throw error 404, 'Entry not found'
	entry

// datetime-local carries no zone; the browser sends its offset alongside.
parseLocal := (value: any, offsetMinutes: any) =>
	s := String(value ?? '').trim()
	return null unless s
	ms := Date.parse(s.length === 16 ? `${s}:00Z` : `${s}Z`)
	return null unless Number.isFinite(ms)
	ms + (Number(offsetMinutes) || 0) * 60000

canRemove := (user: any, entry: any) =>
	can(user, 'content.delete') || (can(user, 'content.edit_own') && entry?.author_id === user?.id && entry?.status !== 'published')

export load := page.load
	handler: async ({ container, params, url, user }) =>
		requireCap user, 'content.view'
		ct := typeOrRedirect container, params.type, url
		entry := await entryOr404 container, ct, params.id
		form := await cms(container).form ct.id
		values := entry ? { ...entry.data, slug: entry.slug, visibility: entry.visibility, markets: entry.markets } : {}
		codes := cms(container).languages()
		languageOf := (code: string) =>
			l := i18n(container).language code
			{ code, name: l.native || l.name || code }
		locale := entry?.locale ?? (codes.includes(url.searchParams.get('lang') ?? '') ? url.searchParams.get('lang') : cms(container).defaultLanguage())
		translations := entry ? await cms(container).translations(entry) : []
		unless entry
			for f of form.fields
				values[f.name] = f.default if f.default !== undefined
		site := await settings(container).get 'site'
		seoCfg := await settings(container).get 'seo'
		author := entry?.author_id ? await auth(container).getUser(entry.author_id) : null
		{
			ct
			entry
			form
			values
			saved: url.searchParams.get('saved') ?? ''
			publicUrl: entry ? cms(container).urlFor(ct, entry) : null
			author: author ? (author.name || author.email) : null
			locale
			languages: codes.map(languageOf)
			translations
			ai: aiAvailable()
			markets: i18n(container).markets().filter((m) => m.languages.some((l) => l.code === locale)).map((m) => ({ code: m.code, name: m.name }))
			revisions: entry ? (await cms(container).revisions(entry.id)).length : 0
			seoContext: { siteTitle: site.title, template: seoCfg.titleTemplate }
			perms: {
				edit: canEditEntry(user, entry)
				publish: can(user, 'content.publish')
				remove: entry ? canRemove(user, entry) : false
			}
		}

// Save the posted fields; returns the result or a fail() for the form.
saveFrom := async (container: any, ct: any, entry: any, raw: any, user: any) =>
	unless canEditEntry(user, entry)
		throw error 403, 'You cannot edit this entry'
	result := await cms(container).save ct.id, entry?.id ?? null, raw, user
	unless result.ok
		return { failed: fail(400, { errors: result.issues, values: raw }) }
	if !entry
		await audit(container).log user, 'content.create', `${ct.id}#${result.id}`, { title: result.entry.title }
	{ result }

export actions := page.actions
	// Actions receive { input, container, event, user }; route params come
	// from event.params.
	save:
		run: async ({ container, event, user }) =>
			ct := typeOr404 container, event.params.type
			entry := await entryOr404 container, ct, event.params.id
			raw := formToObject await event.request.formData()
			{ failed, result } := await saveFrom container, ct, entry, raw, user
			return failed if failed
			throw redirect 303, `/admin/content/${ct.id}/${result.id}?saved=saved`

	// Background save of drafts (never of published entries: that would
	// change the live site without a decision).
	autosave:
		run: async ({ container, event, user }) =>
			ct := typeOr404 container, event.params.type
			entry := await entryOr404 container, ct, event.params.id
			return fail(400, { autosave: 'skipped' }) unless entry && entry.status !== 'published' && canEditEntry(user, entry)
			raw := formToObject await event.request.formData()
			result := await cms(container).save ct.id, entry.id, raw, user
			return fail(400, { autosave: 'invalid' }) unless result.ok
			{ autosaved: Date.now() }

	submit:
		run: async ({ container, event, user }) =>
			ct := typeOr404 container, event.params.type
			entry := await entryOr404 container, ct, event.params.id
			raw := formToObject await event.request.formData()
			{ failed, result } := await saveFrom container, ct, entry, raw, user
			return failed if failed
			await cms(container).setStatus ct.id, result.id, 'in_review', { user }
			await audit(container).log user, 'content.submit', `${ct.id}#${result.id}`, { title: result.entry.title }
			throw redirect 303, `/admin/content/${ct.id}/${result.id}?saved=submitted`

	publish:
		run: async ({ container, event, user }) =>
			requireCap user, 'content.publish'
			ct := typeOr404 container, event.params.type
			entry := await entryOr404 container, ct, event.params.id
			raw := formToObject await event.request.formData()
			{ failed, result } := await saveFrom container, ct, entry, raw, user
			return failed if failed
			at := parseLocal raw.publishAt, raw.tzOffset
			scheduled := !!at && at > Date.now() + 60000
			await cms(container).setStatus ct.id, result.id, 'published', { publishAt: scheduled ? at : (at ?? null), user }
			await audit(container).log user, (scheduled ? 'content.schedule' : 'content.publish'), `${ct.id}#${result.id}`, { title: result.entry.title, at }
			throw redirect 303, `/admin/content/${ct.id}/${result.id}?saved=${scheduled ? 'scheduled' : 'published'}`

	// Create the entry in another language (a linked draft copied from this
	// one) and open it.
	translate:
		run: async ({ container, event, user }) =>
			ct := typeOr404 container, event.params.type
			entry := await entryOr404 container, ct, event.params.id
			unless entry && canEditEntry(user, null)
				throw error 403, 'You cannot create entries'
			fd := await event.request.formData()
			locale := String(fd.get('locale') ?? '')
			data .= null as any
			if fd.get('ai') === '1' && aiAvailable()
				try
					data = await aiTranslateData container, cms(container).fields(ct), entry.data, cms(container).blockLookup, entry.locale, locale
				catch e
					return fail 400, { error: `AI translation failed: ${e.message}` }
			r := await cms(container).translate ct.id, entry.id, locale, user, data
			throw redirect(303, `/admin/content/${ct.id}/${r.existing}`) if !r.ok && r.existing
			return fail(400, { error: r.issues?.[0]?.message ?? 'Could not create the translation' }) unless r.ok
			await audit(container).log user, 'content.translate', `${ct.id}#${entry.id}`, { locale, id: r.id }
			throw redirect 303, `/admin/content/${ct.id}/${r.id}?saved=${data ? 'aitranslated' : 'translated'}`

	current:
		run: async ({ container, event, user }) =>
			ct := typeOr404 container, event.params.type
			entry := await entryOr404 container, ct, event.params.id
			unless entry && canEditEntry(user, entry)
				throw error 403, 'You cannot edit this entry'
			await cms(container).markCurrent entry
			throw redirect 303, `/admin/content/${ct.id}/${entry.id}?saved=current`

	unpublish:
		run: async ({ container, event, user }) =>
			requireCap user, 'content.publish'
			ct := typeOr404 container, event.params.type
			entry := await entryOr404 container, ct, event.params.id
			unless entry
				throw error 404, 'Entry not found'
			await cms(container).setStatus ct.id, entry.id, 'draft', { user }
			await audit(container).log user, 'content.unpublish', `${ct.id}#${entry.id}`, { title: entry.title }
			throw redirect 303, `/admin/content/${ct.id}/${entry.id}?saved=unpublished`

	remove:
		run: async ({ container, event, user }) =>
			ct := typeOr404 container, event.params.type
			entry := await entryOr404 container, ct, event.params.id
			unless entry
				throw error 404, 'Entry not found'
			unless canRemove(user, entry)
				throw error 403, 'You cannot delete this entry'
			await cms(container).remove ct.id, entry.id, user
			await audit(container).log user, 'content.delete', `${ct.id}#${entry.id}`, { title: entry.title }
			throw redirect 303, `/admin/content/${ct.id}`
