import { error, json, redirect } from '@sveltejs/kit'
import { forms } from '$lib/norns/forms/server/public'
import { cms } from '$lib/norns/content/server/public'
import { clientIp } from '$lib/norns/auth/server/public'

// Public form submissions. A browser posts the form here (same-origin, so
// SvelteKit's CSRF check applies). With JavaScript the form asks for JSON
// and shows the answer in place; without it the visitor is sent back to the
// page with ?form=<slug>&sent=1 (or the form's redirect) — or, on errors,
// with ?form=<slug>&error=<message>.
safePage := (p: any) =>
	s := String(p ?? '/')
	(s.startsWith('/') && !s.startsWith('//')) ? s : '/'

export POST := async (event: any) =>
	c := event.locals.container
	fd := await event.request.formData()
	raw := {} as Record<string, any>
	for [k, v] of fd.entries()
		continue unless typeof v === 'string'
		if k in raw
			raw[k] = Array.isArray(raw[k]) ? [...raw[k], v] : [raw[k], v]
		else
			raw[k] = v
	langs := cms(c).languages()
	lang := langs.includes(raw._locale) ? raw._locale : (event.locals.locale?.language ?? cms(c).defaultLanguage())
	page := safePage raw._page
	r := await forms(c).submit event.params.slug, raw, { lang, page, ip: clientIp(event), user: event.locals.user }
	wantsJson := (event.request.headers.get('accept') ?? '').includes('application/json')
	if wantsJson
		return json(r, { status: r.ok ? 200 : (r.status ?? 400) })
	slug := encodeURIComponent event.params.slug
	if r.ok
		throw redirect 303, (r.redirectTo ?? `${page}${page.includes('?') ? '&' : '?'}form=${slug}&sent=1#form-${slug}`)
	message := r.error ?? Object.values(r.errors ?? {})[0] ?? 'Error'
	throw redirect 303, `${page.split('?')[0]}?form=${slug}&error=${encodeURIComponent(message)}#form-${slug}`
