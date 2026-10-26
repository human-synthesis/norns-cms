import { route } from '@human-synthesis/norns/server'
import { consent } from '$lib/norns/consent/server/public'
import { requireCap, audit } from '$lib/norns/auth/server/public'

cell := (v: any) =>
	s := String(v ?? '')
	/[",\n]/.test(s) ? `"${s.replace(/"/g, '""')}"` : s

export GET := route
	handler: async ({ container, user }) =>
		requireCap user, 'consent.view'
		rows := await consent(container).records { limit: 100000 }
		lines := ['created_at,consent_id,action,preferences,analytics,marketing,policy_version,country,region_mode,gpc,banner_hash,member']
		for r of rows
			lines.push [new Date(r.created_at).toISOString(), r.consent_id, r.action, r.categories.preferences, r.categories.analytics, r.categories.marketing, r.policy_version, r.country, r.region_mode, r.gpc, r.banner_hash, r.user_id ? 'yes' : 'no'].map(cell).join(',')
		await audit(container).log user, 'consent.export', `${rows.length} records`, {}
		body := lines.join('\n') + '\n'
		new Response(body, { headers: { 'content-type': 'text/csv; charset=utf-8', 'content-disposition': `attachment; filename="consent-log-${new Date().toISOString().slice(0, 10)}.csv"` } })
