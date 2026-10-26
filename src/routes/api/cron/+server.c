import { route } from '@human-synthesis/norns/server'
import { json, error } from '@sveltejs/kit'
import { timingSafeEqual } from 'node:crypto'
import { envVar, runMaintenance } from '$lib/norns/settings/server/public'

// Daily housekeeping, called by the Worker's scheduled() handler
// (worker/index.js) with the CRON_SECRET secret. Without the secret set,
// the endpoint is disabled.
export POST := route
	handler: async ({ container, event }) =>
		secret := envVar 'CRON_SECRET'
		given := event.request.headers.get('x-cron-key') ?? ''
		unless secret && given.length === secret.length && timingSafeEqual(Buffer.from(given), Buffer.from(secret))
			throw error 403, 'Forbidden'
		json { ok: true, result: await runMaintenance(container) }
