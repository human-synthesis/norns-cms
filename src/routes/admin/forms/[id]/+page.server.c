import { page } from '@human-synthesis/norns/server'
import { error, fail, redirect } from '@sveltejs/kit'
import { forms } from '$lib/norns/forms/server/public'
import { cms } from '$lib/norns/content/server/public'
import { i18n } from '$lib/norns/i18n/server/public'
import { mail } from '$lib/norns/mail/server/public'
import { requireCap, audit } from '$lib/norns/auth/server/public'

formOr404 := async (container: any, id: string) =>
	f := await forms(container).get Number(id)
	unless f
		throw error 404, 'Form not found'
	f

export load := page.load
	handler: async ({ container, params, url, user }) =>
		requireCap user, 'forms.manage'
		f := await formOr404 container, params.id
		def := cms(container).defaultLanguage()
		languages := cms(container).languages().filter((code) => code !== def).map (code) =>
			l := i18n(container).language code
			{ code, name: l.native || l.name || code }
		{ f, languages, created: url.searchParams.has('created'), saved: url.searchParams.has('saved'), provider: mail(container).provider(), defaultNotify: await mail(container).notifyTo() }

// Translations come in as tr_<lang>_<key> and tr_<lang>_field_<name>_<prop>.
translationsFrom := (raw: Record<string, any>, langs: string[]) =>
	out := {} as Record<string, any>
	for code of langs
		tr := { fields: {} } as Record<string, any>
		for key of ['name', 'submitLabel', 'successMessage', 'duplicateMessage']
			v := String(raw[`tr_${code}_${key}`] ?? '').trim()
			tr[key] = v if v
		prefix := `tr_${code}_field_`
		for [k, value] of Object.entries(raw)
			continue unless k.startsWith(prefix)
			v := String(value ?? '').trim()
			continue unless v
			rest := k.slice(prefix.length)
			i := rest.lastIndexOf '_'
			name := rest.slice(0, i)
			prop := rest.slice(i + 1)
			tr.fields[name] = { ...(tr.fields[name] ?? {}), [prop]: v }
		out[code] = tr if Object.keys(tr).length > 1 || Object.keys(tr.fields).length
	out

export actions := page.actions
	save:
		run: async ({ container, event, user }) =>
			requireCap user, 'forms.manage'
			f := await formOr404 container, event.params.id
			fd := await event.request.formData()
			raw := {} as Record<string, any>
			for [k, v] of fd.entries()
				raw[k] = v if typeof v === 'string'
			fields .= null as any
			try
				fields = JSON.parse(raw.fields || '[]').map(({ _key, _named, ...x }) => x)
			catch
				return fail 400, { error: 'The fields could not be read' }
			def := cms(container).defaultLanguage()
			input := {
				name: raw.name
				slug: raw.slug
				enabled: fd.getAll('enabled').at(-1)
				fields
				settings: {
					submitLabel: raw.submitLabel, successMessage: raw.successMessage, duplicateMessage: raw.duplicateMessage
					notify: fd.getAll('notify').at(-1), notifyTo: raw.notifyTo, notifySubject: raw.notifySubject
					dedupe: fd.getAll('dedupe').at(-1), redirectTo: raw.redirectTo
				}
				translations: translationsFrom(raw, cms(container).languages().filter((c) => c !== def))
			}
			r := await forms(container).save f.id, input, user
			return fail(400, { error: r.error }) unless r.ok
			await audit(container).log user, 'forms.save', String(raw.name), {}
			throw redirect 303, `/admin/forms/${f.id}?saved=1`
	remove:
		run: async ({ container, event, user }) =>
			requireCap user, 'forms.manage'
			r := await forms(container).remove Number(event.params.id)
			return fail(400, { error: r.error }) unless r.ok
			await audit(container).log user, 'forms.delete', r.form.name, {}
			throw redirect 303, '/admin/forms'
