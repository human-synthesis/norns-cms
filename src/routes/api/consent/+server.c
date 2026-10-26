import { route } from '@human-synthesis/norns/server'
import { json, error, redirect } from '@sveltejs/kit'
import { consent, COOKIE } from '$lib/norns/consent/server/public'
import { auth, clientIp, safeNext } from '$lib/norns/auth/server/public'

// Records a consent choice and sets the consent cookie. JSON from the
// banner script, or a plain form post without JavaScript (then back to the
// page the visitor was on).
export POST := route
	handler: async ({ container, event, user }) =>
		unless await auth(container).rateLimit('consent:ip', clientIp(event), 30, 60 * 1000)
			throw error 429, 'Too many requests'
		type := event.request.headers.get('content-type') ?? ''
		isJson := type.includes('application/json')
		input := isJson ? await event.request.json().catch(=> ({})) : Object.fromEntries(await event.request.formData())
		r := await consent(container).record event, input, user
		event.cookies.set COOKIE, r.cookie, { path: '/', httpOnly: true, sameSite: 'lax', secure: event.url.protocol === 'https:', maxAge: r.maxAge }
		if isJson
			return json { ok: true, consentId: r.consentId, choice: r.choice }
		throw redirect 303, safeNext(input.back, '/')
