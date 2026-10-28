import type { Container } from '@human-synthesis/norns/server'
import type { FormsRepo } from './repo'
import { TYPE_IDS, WITH_OPTIONS, localizeForm, describeSubmission } from '../shared/forms.js'
import { settings, siteOrigin } from '../../settings/server/public'
import { mail } from '../../mail/server/public'
import { auth } from '../../auth/server/public'
import { i18n, tFor } from '../../i18n/server/public'

DAY := 24 * 60 * 60 * 1000
EMAIL_RE := /^[^\s@]+@[^\s@]+\.[^\s@]+$/
slugOf := (s: string) => String(s ?? '').trim().toLowerCase().replace(/[^a-z0-9_-]+/g, '-').replace(/^-+|-+$/g, '')
nameOf := (s: string) => String(s ?? '').trim().toLowerCase().replace(/[^a-z0-9_]+/g, '_').replace(/^_+|_+$/g, '')
truthy := (raw: any) =>
	v := Array.isArray(raw) ? raw[raw.length - 1] : raw
	v === true || v === 'on' || v === 'true' || v === '1' || v === 1

export class FormsService
	repo: FormsRepo
	c: Container
	constructor(@repo: FormsRepo, @c: Container)

	list()
		@repo.list()

	get(id: number)
		@repo.get id

	bySlug(slug: string)
		@repo.bySlug slug

	// A form for one language (labels, options and messages translated).
	async localized(slug: string, lang: string)
		form := await @repo.bySlug slug
		return null unless form && form.enabled
		localizeForm form, i18n(@c).chain(lang)

	async options()
		(await @repo.list()).map((f: any) => ({ value: f.slug, label: f.name + (f.enabled ? '' : ' (off)') }))

	// --- editing ---------------------------------------------------------------

	normalizeFields(raw: any)
		list := Array.isArray(raw) ? raw : []
		out := [] as any[]
		for f, i of list
			name := nameOf(f.name || f.label)
			return { error: `Field ${i + 1} needs a name` } unless name
			return { error: `Two fields are called "${name}"` } if out.some((x) => x.name === name)
			return { error: `Field "${name}": unknown type` } unless TYPE_IDS.includes(f.type)
			field := { name, label: String(f.label ?? '').trim() || name, type: f.type, required: truthy(f.required) } as Record<string, any>
			field.placeholder = String(f.placeholder) if f.placeholder
			field.help = String(f.help) if f.help
			field.autocomplete = String(f.autocomplete) if f.autocomplete
			if WITH_OPTIONS.has f.type
				opts := (Array.isArray(f.options) ? f.options : String(f.options ?? '').split('\n')).map((o: any) => (typeof o === 'string' ? { value: slugOf(o) || o, label: o.trim() } : { value: String(o.value || slugOf(o.label)), label: String(o.label ?? o.value) })).filter((o: any) => o.label)
				return { error: `Field "${name}" needs at least one option` } unless opts.length
				field.options = opts
			out.push field
		return { error: 'Add at least one field' } unless out.length
		{ fields: out }

	async save(id: number | null, input: any, user: any = null)
		current := id ? await @repo.get(id) : null
		return { ok: false, error: 'Form not found' } if id && !current
		name := String(input.name ?? current?.name ?? '').trim()
		return { ok: false, error: 'Name the form', field: 'name' } unless name
		slug := slugOf(input.slug || current?.slug || name)
		return { ok: false, error: 'Another form uses that URL id', field: 'slug' } if await @repo.slugTaken(slug, id ?? 0)
		fields := @normalizeFields input.fields ?? current?.fields
		return { ok: false, error: fields.error, field: 'fields' } if fields.error
		st := { ...(current?.settings ?? {}), ...(input.settings ?? {}) } as Record<string, any>
		recipients := (Array.isArray(st.notifyTo) ? st.notifyTo : String(st.notifyTo ?? '').split(/[\s,;]+/)).map((x: any) => String(x).trim().toLowerCase()).filter(Boolean)
		bad := recipients.find((x: string) => !EMAIL_RE.test(x))
		return { ok: false, error: `"${bad}" is not an email address`, field: 'notifyTo' } if bad
		settingsOut := {
			submitLabel: String(st.submitLabel ?? '').trim()
			successMessage: String(st.successMessage ?? '').trim()
			duplicateMessage: String(st.duplicateMessage ?? '').trim()
			notify: truthy(st.notify)
			notifyTo: recipients
			notifySubject: String(st.notifySubject ?? '').trim()
			dedupe: truthy(st.dedupe)
			redirectTo: String(st.redirectTo ?? '').trim()
		}
		return { ok: false, error: 'Redirect to a path on this site, e.g. /thanks', field: 'redirectTo' } if settingsOut.redirectTo && !/^\/[^/]/.test(settingsOut.redirectTo)
		translations := (input.translations ?? current?.translations ?? {}) as Record<string, any>
		kind := current?.kind ?? (['contact', 'newsletter'].includes(input.kind) ? input.kind : 'custom')
		enabled := input.enabled === undefined ? (current?.enabled ?? true) : truthy(input.enabled)
		savedId := await @repo.save id, { slug, name, kind, fields: fields.fields, settings: settingsOut, translations, enabled }
		{ ok: true, id: savedId }

	async remove(id: number)
		form := await @repo.get id
		return { ok: false, error: 'Form not found' } unless form
		return { ok: false, error: 'The default contact and newsletter forms can be switched off, not deleted' } if form.kind !== 'custom'
		await @repo.remove id
		{ ok: true, form }

	// --- submitting ------------------------------------------------------------

	// raw: posted values. ctx: { lang, page, ip, user, viaApi, spam } — spam
	// is the Form protection verdict (honeypot / too fast). Returns
	//   { ok: true, message, id, duplicate? } or
	//   { ok: false, errors: { field: message } } / { ok: false, error }.
	async submit(slug: string, raw: Record<string, any>, ctx: Record<string, any>)
		lang := ctx.lang || 'en'
		t := await tFor @c, lang
		form := await @repo.bySlug slug
		return { ok: false, status: 404, error: t('form.unavailable') } unless form && form.enabled
		local := localizeForm form, i18n(@c).chain(lang)
		unless ctx.viaApi
			return { ok: false, status: 429, error: t('form.rate') } unless await auth(@c).rateLimit(`form:${slug}`, ctx.ip || 'unknown', 10, 10 * 60 * 1000)
		spam .= !ctx.viaApi && !!ctx.spam
		data := {} as Record<string, any>
		errors := {} as Record<string, string>
		email .= ''
		for f of local.fields
			value := raw[f.name]
			if f.type === 'checkbox' || f.type === 'consent'
				on := truthy value
				errors[f.name] = t((f.type === 'consent' ? 'form.tick' : 'form.required'), { field: f.label }) if f.required && !on
				data[f.name] = on
				data[`${f.name}__text`] = f.label if f.type === 'consent' && on
				continue
			if f.type === 'checkboxes'
				list := (Array.isArray(value) ? value : (value ? [value] : [])).map(String).filter((v: string) => f.options.some((o: any) => o.value === v))
				errors[f.name] = t('form.required', { field: f.label }) if f.required && !list.length
				data[f.name] = list
				continue
			text := String(Array.isArray(value) ? value[value.length - 1] : (value ?? '')).trim().slice(0, f.type === 'textarea' ? 10000 : 500)
			if !text
				errors[f.name] = t('form.required', { field: f.label }) if f.required
				data[f.name] = ''
				continue
			if f.type === 'email'
				if EMAIL_RE.test(text)
					email ||= text.toLowerCase()
				else
					errors[f.name] = t('form.invalidEmail')
			else if (f.type === 'select' || f.type === 'radio') && !f.options.some((o: any) => o.value === text)
				errors[f.name] = t('form.invalid', { field: f.label })
			else if f.type === 'number' && !Number.isFinite(Number(text))
				errors[f.name] = t('form.invalid', { field: f.label })
			else if f.type === 'url' && !/^https?:\/\/\S+$/i.test(text)
				errors[f.name] = t('form.invalid', { field: f.label })
			data[f.name] = text
		return { ok: false, status: 400, errors } if Object.keys(errors).length
		links := (Object.values(data).filter((v) => typeof v === 'string').join(' ').match(/https?:\/\//g) ?? []).length
		spam = true if links > 3
		success := local.successMessage || t('form.sent')
		if form.settings.dedupe && email && await @repo.hasEmail(form.id, email)
			return { ok: true, duplicate: true, message: local.duplicateMessage || success, redirectTo: form.settings.redirectTo || null }
		id := await @repo.insertSubmission { formId: form.id, data, email, status: (spam ? 'spam' : 'new'), locale: lang, page: String(ctx.page ?? '').slice(0, 300), userId: ctx.user?.id ?? null }
		await @notify(form, local, id, data, email, lang) if !spam && form.settings.notify
		{ ok: true, id, message: success, redirectTo: form.settings.redirectTo || null }

	// A notification to the team: the form's own recipients, else Settings →
	// Email → Notification address. Replies go to the person who wrote.
	async notify(form: any, local: any, id: number, data: Record<string, any>, email: string, lang: string)
		recipients := form.settings.notifyTo?.length ? form.settings.notifyTo : await mail(@c).notifyTo()
		unless recipients.length
			await @repo.setNotified id, -1, 'No notification address (Settings → Email)'
			return
		site := await settings(@c).get 'site'
		fill := (s: string) => s.replace(/\{(\w+)\}/g, (m, k) => (data[k] == null ? m : String(data[k])))
		subject := fill(form.settings.notifySubject || `New submission: ${form.name}`) + ` — ${site.title}`
		body := [`${form.name} (${lang})`, '', describeSubmission(form.fields, data), '', `Open it: ${siteOrigin()}/admin/forms/submissions/${id}`].join('\n')
		errorsSeen := [] as string[]
		purpose := form.slug === 'contact' ? 'contact' : 'notifications'
		for to of recipients
			r := await mail(@c).send { purpose, to, subject, text: body, template: `form:${form.slug}`, replyTo: email || undefined }
			errorsSeen.push(`${to}: ${r.error}`) unless r.ok
		await @repo.setNotified id, (errorsSeen.length ? -1 : 1), errorsSeen.join('; ').slice(0, 500)
		return

	// --- inbox -----------------------------------------------------------------

	submissions(q: any = {})
		@repo.submissions q

	count(q: any = {})
		@repo.count q

	submission(id: number)
		@repo.submission id

	async setStatus(ids: number[], status: string)
		return { ok: false, error: 'Unknown status' } unless ['new', 'read', 'archived', 'spam'].includes(status)
		await @repo.setStatus ids, status
		{ ok: true }

	async removeSubmissions(ids: number[])
		await @repo.removeSubmissions ids
		{ ok: true }

	async csv(formId: number)
		form := await @repo.get formId
		return null unless form
		rows := await @repo.submissions { formId, limit: 100000 }
		cell := (v: any) =>
			s := Array.isArray(v) ? v.join('; ') : (typeof v === 'boolean' ? (v ? 'yes' : 'no') : String(v ?? ''))
			/[",\n]/.test(s) ? `"${s.replace(/"/g, '""')}"` : s
		head := ['submitted_at', 'status', 'language', 'page', ...form.fields.map((f: any) => f.name)]
		lines := [head.join(',')]
		for r of rows
			lines.push [new Date(r.created_at).toISOString(), r.status, r.locale, r.page, ...form.fields.map((f: any) => r.data[f.name])].map(cell).join(',')
		{ name: form.slug, csv: lines.join('\n') + '\n' }

	async purge()
		cfg := await settings(@c).get 'forms'
		days := Math.max 1, Number(cfg.retentionDays) || 730
		await @repo.purge Date.now() - days * DAY
