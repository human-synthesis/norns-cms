import { page } from '@human-synthesis/norns/server'
import { auth, clientIp } from '$lib/norns/auth/server/public'
import { protect } from '$lib/norns/protection/server/public'

export load := page.load
	handler: => ({ seo: { robots: 'noindex, follow' } })

pageActions := page.actions
	request:
		run: async ({ container, event }) =>
			await auth(container).requestReset String((await event.request.formData()).get('email') ?? ''), clientIp(event)
			{ sent: true }

// Honeypot, signed render time and Turnstile (Admin → Form protection).
export actions := protect pageActions, { request: 'auth.member-forgot' }
