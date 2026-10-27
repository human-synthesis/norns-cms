import type { Container } from '@human-synthesis/norns/server'
import { registerEndpoint, apiError, paging, need } from '../../api/server/public'
import { forms } from './public'
import { audit } from '../../auth/server/public'

formOf := async (c: Container, id: string) =>
	f := await forms(c).get Number(id)
	need f, 'Form not found'

export registerFormsApi := (app: Container) =>
	registerEndpoint app, {
		method: 'GET', path: '/forms', capability: 'forms.view', tag: 'Forms'
		summary: 'Forms with their fields, options, translations and unread counts'
		handler: ({ c }) => forms(c).list()
	}
	registerEndpoint app, {
		method: 'GET', path: '/forms/:id', capability: 'forms.view', tag: 'Forms'
		summary: 'One form'
		handler: ({ c, params }) => formOf c, params.id
	}
	registerEndpoint app, {
		method: 'POST', path: '/forms', capability: 'forms.manage', tag: 'Forms', created: true
		summary: 'Create a form'
		body: { description: 'name, slug?, fields [{ name, label, type, required, options?, placeholder?, help? }], settings { submitLabel, successMessage, duplicateMessage, notify, notifyTo, notifySubject, dedupe, redirectTo }, translations { <lang>: { name, submitLabel, successMessage, fields: { <name>: { label, placeholder, help } } } }, enabled', example: { name: 'Quote request', fields: [{ name: 'email', label: 'Email', type: 'email', required: true }, { name: 'details', label: 'What do you need?', type: 'textarea', required: true }], settings: { notify: true, notifySubject: 'Quote request from {email}' } } }
		handler: async ({ c, body, user }) =>
			r := await forms(c).save null, body, user
			throw apiError(400, r.error, { field: r.field }) unless r.ok
			await audit(c).log user, 'forms.save', String(body.name), { via: 'api' }
			await forms(c).get r.id
	}
	registerEndpoint app, {
		method: 'PATCH', path: '/forms/:id', capability: 'forms.manage', tag: 'Forms'
		summary: 'Change a form (fields given replace the current ones; settings and translations are merged)'
		body: { description: 'Any form field', example: { enabled: false } }
		handler: async ({ c, params, body, user }) =>
			current := await formOf c, params.id
			input := { ...body, settings: { ...current.settings, ...(body.settings ?? {}) }, translations: body.translations ? { ...current.translations, ...body.translations } : current.translations }
			r := await forms(c).save current.id, input, user
			throw apiError(400, r.error, { field: r.field }) unless r.ok
			await audit(c).log user, 'forms.save', current.name, { via: 'api' }
			await forms(c).get current.id
	}
	registerEndpoint app, {
		method: 'DELETE', path: '/forms/:id', capability: 'forms.manage', tag: 'Forms'
		summary: 'Delete a custom form and its submissions'
		handler: async ({ c, params, user }) =>
			r := await forms(c).remove Number(params.id)
			throw apiError(400, r.error) unless r.ok
			await audit(c).log user, 'forms.delete', r.form.name, { via: 'api' }
			{ deleted: Number(params.id) }
	}
	registerEndpoint app, {
		method: 'POST', path: '/forms/:slug/submissions', capability: 'forms.manage', tag: 'Forms', created: true
		summary: 'Submit a form on someone\'s behalf (validated and notified like a visitor\'s submission)'
		body: { description: 'Field values by name; locale? for messages and notifications', example: { email: 'ada@example.com', consent: true } }
		handler: async ({ c, params, body, user }) =>
			r := await forms(c).submit params.slug, body, { lang: body.locale || 'en', page: 'api', user, viaApi: true }
			throw apiError((r.status ?? 400), r.error ?? 'Invalid input', { errors: r.errors }) unless r.ok
			r
	}
	registerEndpoint app, {
		method: 'GET', path: '/submissions', capability: 'forms.view', tag: 'Forms'
		summary: 'Form submissions, newest first (spam only with status=spam)'
		query: [{ name: 'form', description: 'Form id' }, { name: 'status', description: 'new | read | archived | spam' }, { name: 'q', description: 'Search email and values' }, { name: 'limit', description: '1–500' }, { name: 'offset', description: 'Default 0' }]
		handler: async ({ c, query }) =>
			{ limit, offset } := paging query, 500
			q := { formId: Number(query.form) || undefined, status: query.status || undefined, q: query.q || undefined }
			{ data: await forms(c).submissions({ ...q, limit, offset }), meta: { total: await forms(c).count(q), limit, offset } }
	}
	registerEndpoint app, {
		method: 'GET', path: '/submissions/:id', capability: 'forms.view', tag: 'Forms'
		summary: 'One submission'
		handler: async ({ c, params }) =>
			s := await forms(c).submission Number(params.id)
			need s, 'Submission not found'
	}
	registerEndpoint app, {
		method: 'PATCH', path: '/submissions/:id', capability: 'forms.manage', tag: 'Forms'
		summary: 'Set the status of a submission'
		body: { description: 'status: new | read | archived | spam', example: { status: 'archived' } }
		handler: async ({ c, params, body }) =>
			r := await forms(c).setStatus [Number(params.id)], String(body.status ?? '')
			throw apiError(400, r.error) unless r.ok
			await forms(c).submission Number(params.id)
	}
	registerEndpoint app, {
		method: 'DELETE', path: '/submissions/:id', capability: 'forms.manage', tag: 'Forms'
		summary: 'Delete a submission'
		handler: async ({ c, params, user }) =>
			await forms(c).removeSubmissions [Number(params.id)]
			await audit(c).log user, 'forms.submission_delete', `#${params.id}`, { via: 'api' }
			{ deleted: Number(params.id) }
	}
