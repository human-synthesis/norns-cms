import { page } from '@human-synthesis/norns/server'
import { auth, clientIp } from '$lib/norns/auth/server/public'

export load := page.load
	handler: => ({ seo: { title: 'Reset your password', robots: 'noindex, follow' } })

export actions := page.actions
	request:
		run: async ({ container, event }) =>
			await auth(container).requestReset String((await event.request.formData()).get('email') ?? ''), clientIp(event)
			{ sent: true }
