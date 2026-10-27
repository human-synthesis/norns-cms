import { page } from '@human-synthesis/norns/server'
import { fail, redirect } from '@sveltejs/kit'
import { auth, COOKIE } from '$lib/norns/auth/server/public'
import { tFor } from '$lib/norns/i18n/server/public'

err := (field: string, message: string, extra: any = {}) => fail(400, { errors: [{ path: [{ key: field }], message }], ...extra })

// The member's own account. sessionHandle guarantees a signed-in client here.
export load := page.load
	handler: async ({ container, user }) =>
		{ u: await auth(container).getUser(user.id), seo: { robots: 'noindex, nofollow' } }

export actions := page.actions
	profile:
		run: async ({ container, event, user }) =>
			fd := await event.request.formData()
			values := { name: String(fd.get('name') ?? ''), email: String(fd.get('email') ?? '') }
			r := await auth(container).updateUser user.id, { ...values, locale: undefined }, user
			return err(r.field ?? 'name', r.error, { values }) unless r.ok
			t := await tFor container, event.locals.locale.language
			{ saved: t('account.saved') }
	password:
		run: async ({ container, event, user }) =>
			fd := await event.request.formData()
			next := String(fd.get('next') ?? '')
			t := await tFor container, event.locals.locale.language
			return err('confirm', t('account.mismatch')) unless next === String(fd.get('confirm') ?? '')
			r := await auth(container).changePassword user, String(fd.get('current') ?? ''), next
			return err((r.field === 'password' ? 'next' : r.field) ?? 'current', r.error) unless r.ok
			{ saved: t('account.changed') }
	logout:
		run: async ({ container, event }) =>
			sid := event.cookies.get COOKIE
			await auth(container).logout(sid) if sid
			event.cookies.delete COOKIE, { path: '/' }
			throw redirect 303, '/'
