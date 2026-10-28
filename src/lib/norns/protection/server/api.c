import type { Container } from '@human-synthesis/norns/server'
import { registerEndpoint, apiError } from '../../api/server/public'
import { protection } from './public'

export registerProtectionApi := (app: Container) =>
	registerEndpoint app, {
		method: 'GET', path: '/protection', capability: 'protection.manage', tag: 'Form protection'
		summary: 'Turnstile configuration (never the secret key) and whether it is active'
		handler: ({ c }) => protection(c).config()
	}
	registerEndpoint app, {
		method: 'PATCH', path: '/protection', capability: 'protection.manage', tag: 'Form protection'
		summary: 'Set the Turnstile keys and switches; a new secret key is checked with Cloudflare and stored encrypted'
		body: { description: 'enabled?, siteKey?, secretKey? (write-only), clearSecret?, appearance? (always | interaction-only)', example: { enabled: true, siteKey: '0x4AAAAAAA…', secretKey: '0x4AAAAAAA…', appearance: 'interaction-only' } }
		handler: async ({ c, body, user }) =>
			r := await protection(c).saveConfig body ?? {}, user, { via: 'api' }
			throw apiError(400, r.error, { field: r.field }) unless r.ok
			{ ...(await protection(c).config()), warning: r.warning || undefined }
	}
	registerEndpoint app, {
		method: 'GET', path: '/protection/forms', capability: 'protection.manage', tag: 'Form protection'
		summary: 'Every registered form with its Turnstile and honeypot switches and the last 30 days of outcomes'
		handler: ({ c }) => protection(c).forms()
	}
	registerEndpoint app, {
		method: 'PATCH', path: '/protection/forms/:id', capability: 'protection.manage', tag: 'Form protection'
		summary: 'Change one form’s protection'
		body: { description: 'turnstile? (default | on | off), honeypot? (boolean)', example: { turnstile: 'on', honeypot: true } }
		handler: async ({ c, params, body, user }) =>
			r := await protection(c).saveForm params.id, body ?? {}, user, { via: 'api' }
			throw apiError((r.error === 'Form not found' ? 404 : 400), r.error, { field: r.field }) unless r.ok
			(await protection(c).forms()).find (f) => f.id === params.id
	}
