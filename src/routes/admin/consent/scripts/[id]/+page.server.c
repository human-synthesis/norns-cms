import { page } from '@human-synthesis/norns/server'
import { error, fail, redirect } from '@sveltejs/kit'
import { consent } from '$lib/norns/consent/server/public'
import { requireCap, can, audit } from '$lib/norns/auth/server/public'

export load := page.load
	handler: async ({ container, params, user }) =>
		requireCap user, 'consent.manage'
		script := params.id === 'new' ? null : await consent(container).script(Number(params.id))
		if params.id !== 'new' && !script
			throw error 404, 'Script not found'
		{ script, providers: consent(container).providers(), canCustom: can(user, 'consent.scripts_custom') }

export actions := page.actions
	save:
		run: async ({ container, event, user }) =>
			requireCap user, 'consent.manage'
			raw := Object.fromEntries(await event.request.formData()) as Record<string, any>
			id := event.params.id === 'new' ? null : Number(event.params.id)
			existing := id ? await consent(container).script(id) : null
			// Custom code is arbitrary script on the public site: its own capability.
			if (raw.provider === 'custom' || existing?.provider === 'custom') && !can(user, 'consent.scripts_custom')
				throw error 403, 'Adding custom code needs the consent.scripts_custom capability'
			r := await consent(container).saveScript id, raw, user
			return fail(400, { errors: [{ path: [{ key: r.field ?? 'name' }], message: r.error }], values: raw }) unless r.ok
			await audit(container).log user, 'consent.script_save', r.name, { provider: r.provider, before: existing?.provider === 'custom' ? existing.code.slice(0, 2000) : undefined, after: raw.provider === 'custom' ? String(raw.code).slice(0, 2000) : undefined }
			throw redirect 303, `/admin/consent/scripts/${r.id}?saved=1`
	remove:
		run: async ({ container, event, user }) =>
			requireCap user, 'consent.manage'
			id := Number(event.params.id)
			s := await consent(container).script id
			await consent(container).removeScript id
			await audit(container).log user, 'consent.script_delete', s?.name ?? `script#${id}`, {}
			throw redirect 303, '/admin/consent/scripts'
