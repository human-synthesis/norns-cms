import { page } from '@human-synthesis/norns/server'
import { fail, redirect } from '@sveltejs/kit'
import { forms } from '$lib/norns/forms/server/public'
import { requireCap, can, audit } from '$lib/norns/auth/server/public'

export load := page.load
	handler: async ({ container, user }) =>
		requireCap user, 'forms.view'
		{ forms: await forms(container).list(), canManage: can(user, 'forms.manage') }

export actions := page.actions
	create:
		run: async ({ container, event, user }) =>
			requireCap user, 'forms.manage'
			name := String((await event.request.formData()).get('name') ?? '').trim()
			r := await forms(container).save null, { name, fields: [{ name: 'email', label: 'Email', type: 'email', required: true }], settings: { notify: true } }, user
			return fail(400, { error: r.error }) unless r.ok
			await audit(container).log user, 'forms.save', name, {}
			throw redirect 303, `/admin/forms/${r.id}?created=1`
