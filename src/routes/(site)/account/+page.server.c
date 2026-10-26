import { page } from '@human-synthesis/norns/server'
import { fail, redirect } from '@sveltejs/kit'
import { auth, COOKIE } from '$lib/norns/auth/server/public'

err := (field: string, message: string, extra: any = {}) => fail(400, { errors: [{ path: [{ key: field }], message }], ...extra })

// The member's own account. sessionHandle guarantees a signed-in client here.
export load := page.load
	handler: async ({ container, user }) =>
		{ u: await auth(container).getUser(user.id), seo: { title: 'Your account', robots: 'noindex, nofollow' } }

export actions := page.actions
	profile:
		run: async ({ container, event, user }) =>
			fd := await event.request.formData()
			values := { name: String(fd.get('name') ?? ''), email: String(fd.get('email') ?? '') }
			r := await auth(container).updateUser user.id, values, user
			return err(r.field ?? 'name', r.error, { values }) unless r.ok
			{ saved: 'Saved.' }
	password:
		run: async ({ container, event, user }) =>
			fd := await event.request.formData()
			next := String(fd.get('next') ?? '')
			return err('confirm', 'The new passwords do not match') unless next === String(fd.get('confirm') ?? '')
			r := await auth(container).changePassword user, String(fd.get('current') ?? ''), next
			return err((r.field === 'password' ? 'next' : r.field) ?? 'current', r.error) unless r.ok
			{ saved: 'Password changed. Other devices were signed out.' }
	logout:
		run: async ({ container, event }) =>
			sid := event.cookies.get COOKIE
			await auth(container).logout(sid) if sid
			event.cookies.delete COOKIE, { path: '/' }
			throw redirect 303, '/'
