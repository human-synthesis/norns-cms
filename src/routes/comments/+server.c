import { json, redirect } from '@sveltejs/kit'
import { comments } from '$lib/norns/comments/server/public'
import { cms } from '$lib/norns/content/server/public'
import { clientIp } from '$lib/norns/auth/server/public'
import { protection } from '$lib/norns/protection/server/public'

// Public comments. The comment form posts here (same-origin, so SvelteKit's
// CSRF check applies). With JavaScript it asks for JSON; without, the
// visitor goes back to the page with ?comment=<id>&commented=<status>, or on
// errors with ?commentError=<message> (and their text kept by the browser's
// back history only — the form is short).
safePage := (p: any) =>
	s := String(p ?? '/')
	(s.startsWith('/') && !s.startsWith('//')) ? s : '/'

export POST := async (event: any) =>
	c := event.locals.container
	fd := await event.request.formData()
	raw := {} as Record<string, string>
	for [k, v] of fd.entries()
		raw[k] = v if typeof v === 'string'
	langs := cms(c).languages()
	lang := langs.includes(raw._locale) ? raw._locale : (event.locals.locale?.language ?? cms(c).defaultLanguage())
	page := safePage(raw._page).split('#')[0].split('?')[0]
	wantsJson := (event.request.headers.get('accept') ?? '').includes('application/json')
	// A honeypot or too-fast hit is kept as spam (the writer sees success);
	// a failed Turnstile or token is an error.
	v := await protection(c).check event, 'comments.post', fd
	r := if v.ok || v.silent
		await comments(c).post { entryId: raw.entry, parentId: raw.parent || null, body: raw.body, name: raw.name, email: raw.email }, { user: event.locals.user, ip: clientIp(event), lang, spam: !v.ok }
	else
		{ ok: false, status: 400, error: await protection(c).message(v.reason, { locals: { locale: { language: lang } } }, 'comments.post') }
	if wantsJson
		return json(r, { status: r.ok ? 200 : (r.status ?? 400) })
	if r.ok
		throw redirect 303, `${page}?comment=${r.id}&commented=${r.status === 'approved' ? 'approved' : 'pending'}#comment-${r.id}`
	message := r.error ?? Object.values(r.errors ?? {})[0] ?? 'Error'
	reply := raw.parent ? `&reply=${encodeURIComponent(raw.parent)}` : ''
	throw redirect 303, `${page}?commentError=${encodeURIComponent(message)}${reply}#respond`
