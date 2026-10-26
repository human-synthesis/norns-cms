import { page } from '@human-synthesis/norns/server'
import { auth, clientIp } from '$lib/norns/auth/server/public'

export load := page.load
	handler: => ({})

export actions := page.actions
	request:
		run: async ({ container, event }) =>
			email := String((await event.request.formData()).get('email') ?? '')
			await auth(container).requestReset email, clientIp(event)
			{ sent: true }
