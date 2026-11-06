import { route } from '@human-synthesis/norns/server'
import { json, error } from '@sveltejs/kit'
import { timingSafeEqual } from 'node:crypto'
import { envVar, runMaintenance } from '$lib/norns/settings/server/public'
import { seo } from '$lib/norns/seo/server/public'

// The daily trigger in wrangler.toml; every other trigger (half-hourly)
// only runs the search engine notifications.
DAILY := '17 3 * * *'

// Called by the Worker's scheduled() handler (added to worker/index.js by
// tools/worker-cron.js after the build) with the CRON_SECRET secret and
// the trigger's expression in x-cron. Without the secret set, the
// endpoint is disabled. A call without x-cron runs everything.
export POST := route
	handler: async ({ container, event }) =>
		secret := envVar 'CRON_SECRET'
		given := event.request.headers.get('x-cron-key') ?? ''
		unless secret && given.length === secret.length && timingSafeEqual(Buffer.from(given), Buffer.from(secret))
			throw error 403, 'Forbidden'
		cron := event.request.headers.get('x-cron') ?? ''
		out := {} as Record<string, any>
		out.maintenance = await runMaintenance(container) if !cron || cron === DAILY
		try
			out.indexing = await seo(container).indexingTick()
		catch e
			out.indexing = { error: String(e) }
		json { ok: true, cron, result: out }
