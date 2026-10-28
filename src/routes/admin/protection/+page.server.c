import { page } from '@human-synthesis/norns/server'
import { fail } from '@sveltejs/kit'
import { requireCap } from '$lib/norns/auth/server/public'
import { protection } from '$lib/norns/protection/server/public'

// Cloudflare Turnstile keys and the protection of every registered form.
export load := page.load
	handler: async ({ container, user }) =>
		requireCap user, 'protection.manage'
		{ config: await protection(container).config(), forms: await protection(container).forms() }

export actions := page.actions
	keys:
		run: async ({ container, event, user }) =>
			requireCap user, 'protection.manage'
			fd := await event.request.formData()
			input := {
				enabled: fd.get('enabled') === 'on'
				siteKey: String(fd.get('siteKey') ?? '')
				secretKey: String(fd.get('secretKey') ?? '')
				clearSecret: fd.get('clearSecret') === 'on'
				appearance: String(fd.get('appearance') ?? 'always')
			}
			r := await protection(container).saveConfig input, user
			return fail(400, { errors: [{ path: [{ key: r.field ?? 'siteKey' }], message: r.error }], keysForm: true }) unless r.ok
			{ saved: r.warning || 'Turnstile settings saved.' }
	forms:
		run: async ({ container, event, user }) =>
			requireCap user, 'protection.manage'
			fd := await event.request.formData()
			for f of await protection(container).forms()
				continue unless fd.has(`turnstile:${f.id}`)
				await protection(container).saveForm f.id, { turnstile: String(fd.get(`turnstile:${f.id}`)), honeypot: fd.get(`honeypot:${f.id}`) === 'on' }, user
			{ saved: 'Form protection saved.' }
