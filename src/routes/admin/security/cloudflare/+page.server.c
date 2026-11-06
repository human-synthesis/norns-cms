import { page } from '@human-synthesis/norns/server'
import { fail } from '@sveltejs/kit'
import { cloudflare } from '$lib/norns/protection/server/public'
import { requireCap, audit } from '$lib/norns/auth/server/public'

export load := page.load
	handler: async ({ container, user }) =>
		requireCap user, 'protection.manage'
		{ status: await cloudflare(container).status(), preview: await cloudflare(container).preview() }

syncMessage := (r: any) =>
	return '' unless r
	r.ok ? `Cloudflare updated: ${r.rules.length} rule${r.rules.length === 1 ? '' : 's'} on ${r.zone}.` : `Saved, but Cloudflare refused the rules: ${r.error}`

export actions := page.actions
	save:
		run: async ({ container, event, user }) =>
			requireCap user, 'protection.manage'
			fd := await event.request.formData()
			input := { token: String(fd.get('token') ?? ''), clearToken: fd.has('clearToken'), zoneId: String(fd.get('zoneId') ?? ''), paths: fd.has('paths'), countries: fd.has('countries'), auto: fd.has('auto') }
			r := await cloudflare(container).save input, user
			return fail(400, { error: r.error }) unless r.ok
			await audit(container).log user, 'protection.cloudflare', '', { token: !!input.token, cleared: input.clearToken, synced: r.synced?.ok ?? null }
			r.synced && !r.synced.ok ? { warning: syncMessage(r.synced) } : { saved: syncMessage(r.synced) || 'Saved.' }
	sync:
		run: async ({ container, user }) =>
			requireCap user, 'protection.manage'
			r := await cloudflare(container).sync()
			await audit(container).log user, 'protection.cloudflare_sync', '', { ok: r.ok }
			return fail(502, { error: `Cloudflare refused: ${r.error}` }) unless r.ok
			{ saved: syncMessage(r) }
	remove:
		run: async ({ container, user }) =>
			requireCap user, 'protection.manage'
			r := await cloudflare(container).remove()
			await audit(container).log user, 'protection.cloudflare_remove', '', { ok: r.ok }
			return fail(502, { error: `Cloudflare refused: ${r.error}` }) unless r.ok
			{ saved: "The CMS's rules were removed from Cloudflare. Turn automatic sync off too, or the next change puts them back." }
