import type { Container } from '@human-synthesis/norns/server'

// Housekeeping across features, run by the daily cron trigger (POST
// /api/cron with CRON_SECRET) or from Admin → Maintenance. Each feature
// exposes `purge()` on its facade; they are resolved by token so settings
// does not import every feature.
export runMaintenance := async (c: Container) =>
	out := {} as Record<string, any>
	tasks := [
		['users', 'auth.service']
		['consent', 'consent.service']
		['seo', 'seo.service']
		['forms', 'forms.service']
		['protection', 'protection.service']
	]
	for [name, token] of tasks
		try
			out[name] = await (c.resolve(token) as any).purge()
		catch e
			out[name] = { error: String(e) }
	try
		out.mail = await (c.resolve('mail.service') as any).purge(Date.now() - 90 * 24 * 60 * 60 * 1000)
	catch e
		out.mail = { error: String(e) }
	out
