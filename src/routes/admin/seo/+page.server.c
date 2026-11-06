import { page } from '@human-synthesis/norns/server'
import { seo } from '$lib/norns/seo/server/public'
import { requireCap, audit, can } from '$lib/norns/auth/server/public'
import { looksLikeProbe, suggestPattern } from '$lib/norns/protection/server/public'
import { siteOrigin } from '$lib/norns/settings/server/public'

export load := page.load
	handler: async ({ container, user }) =>
		requireCap user, 'seo.manage'
		{ report: await seo(container).report(), notFound: (await seo(container).notFound()).map((n) => ({ ...n, probe: looksLikeProbe(n.path), pattern: suggestPattern(n.path) })), canBlock: can(user, 'protection.manage'), indexing: await seo(container).indexingStatus(), origin: siteOrigin() }

export actions := page.actions
	clear404:
		run: async ({ container, event, user }) =>
			requireCap user, 'seo.manage'
			path := String((await event.request.formData()).get('path') ?? '')
			await seo(container).clearNotFound(path || null)
			{ cleared: true }
	saveRules:
		run: async ({ container, event, user }) =>
			requireCap user, 'seo.manage'
			fd := await event.request.formData()
			current := await seo(container).indexingRules()
			auto := {} as Record<string, any>
			for id of Object.keys(current.auto)
				auto[id] = { new: fd.has(`auto.${id}.new`), updated: fd.has(`auto.${id}.updated`), removed: fd.has(`auto.${id}.removed`) }
			await seo(container).saveIndexingRules { enabled: fd.has('enabled'), auto, comments: fd.has('comments') }
			await audit(container).log user, 'seo.indexing_rules', '', {}
			{ saved: 'IndexNow settings saved.' }
	submitUrls:
		run: async ({ container, event, user }) =>
			requireCap user, 'seo.manage'
			r := await seo(container).submitUrls String((await event.request.formData()).get('urls') ?? '')
			return { failed: r.reason } if r.reason
			await audit(container).log user, 'seo.indexing_submit', `${r.sent} URLs`, {}
			skipped := r.skipped.length ? ` Skipped ${r.skipped.length} not on this site's public hosts.` : ''
			{ saved: `Sent ${r.sent} URL${r.sent === 1 ? '' : 's'} to IndexNow.${skipped}` }
	submitAll:
		run: async ({ container, user }) =>
			requireCap user, 'seo.manage'
			r := await seo(container).submitAll()
			return { failed: r.reason } if r.reason
			await audit(container).log user, 'seo.indexing_submit', `${r.sent} URLs`, {}
			{ saved: r.sent ? `Sent ${r.sent} URL${r.sent === 1 ? '' : 's'} to IndexNow.` : 'Nothing to send: no public URLs on a live https host.' }
	submitGoogle:
		run: async ({ container, user }) =>
			requireCap user, 'seo.manage'
			r := await seo(container).submitSitemaps()
			return { failed: r.skipped } if r.skipped
			await audit(container).log user, 'seo.indexing_google', '', { ok: r.ok }
			r.ok ? { saved: 'Sitemaps submitted to Google.' } : { failed: 'Google refused the sitemap; see the log below.' }
	block:
		run: async ({ container, event, user }) =>
			requireCap user, 'protection.manage'
			pattern := String((await event.request.formData()).get('pattern') ?? '')
			r := await seo(container).blockFromLog pattern, user
			return { failed: r.error } unless r.ok
			await audit(container).log user, 'protection.block_path', r.pattern, { from: '404', cleared: r.cleared }
			{ saved: `Blocked ${r.pattern}; ${r.cleared} path${r.cleared === 1 ? '' : 's'} left the 404 log.` }
