import { page } from '@human-synthesis/norns/server'
import { error, fail, redirect } from '@sveltejs/kit'
import { forms } from '$lib/norns/forms/server/public'
import { requireCap, can, audit } from '$lib/norns/auth/server/public'

subOr404 := async (container: any, id: string) =>
	s := await forms(container).submission Number(id)
	unless s
		throw error 404, 'Submission not found'
	s

export load := page.load
	handler: async ({ container, params, user }) =>
		requireCap user, 'forms.view'
		s := await subOr404 container, params.id
		// Opening a new submission marks it read.
		if s.status === 'new' && can(user, 'forms.manage')
			await forms(container).setStatus [s.id], 'read'
		f := await forms(container).get s.form_id
		{ s, f, canManage: can(user, 'forms.manage') }

export actions := page.actions
	status:
		run: async ({ container, event, user }) =>
			requireCap user, 'forms.manage'
			status := String((await event.request.formData()).get('status') ?? '')
			r := await forms(container).setStatus [Number(event.params.id)], status
			return fail(400, { error: r.error }) unless r.ok
			{ saved: status }
	remove:
		run: async ({ container, event, user }) =>
			requireCap user, 'forms.manage'
			await forms(container).removeSubmissions [Number(event.params.id)]
			await audit(container).log user, 'forms.submission_delete', `#${event.params.id}`, {}
			throw redirect 303, '/admin/forms/submissions'
