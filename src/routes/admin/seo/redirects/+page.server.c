import { page } from '@human-synthesis/norns/server'
import { fail } from '@sveltejs/kit'
import { seo } from '$lib/norns/seo/server/public'
import { requireCap, audit } from '$lib/norns/auth/server/public'

export load := page.load
	handler: async ({ container, url, user }) =>
		requireCap user, 'seo.manage'
		q := url.searchParams.get('q') ?? ''
		{ redirects: await seo(container).redirects(q), q, from: url.searchParams.get('from') ?? '' }

export actions := page.actions
	create:
		run: async ({ container, event, user }) =>
			requireCap user, 'seo.manage'
			values := Object.fromEntries(await event.request.formData()) as Record<string, string>
			r := await seo(container).saveRedirect null, values
			return fail(400, { errors: [{ path: [{ key: r.field ?? 'from' }], message: r.error }], values }) unless r.ok
			await audit(container).log user, 'seo.redirect', values.from, { to: values.to, status: values.status }
			{ saved: 'Redirect saved.' }
	remove:
		run: async ({ container, event, user }) =>
			requireCap user, 'seo.manage'
			id := Number((await event.request.formData()).get('id'))
			await seo(container).removeRedirect id
			await audit(container).log user, 'seo.redirect_delete', `redirect#${id}`, {}
			{ saved: 'Redirect removed.' }
	import:
		run: async ({ container, event, user }) =>
			requireCap user, 'seo.manage'
			text := String((await event.request.formData()).get('csv') ?? '')
			r := await seo(container).importRedirects text
			await audit(container).log user, 'seo.redirect_import', `${r.imported} rows`, { errors: r.errors.length }
			{ saved: `Imported ${r.imported} redirect${r.imported === 1 ? '' : 's'}.`, importErrors: r.errors }
