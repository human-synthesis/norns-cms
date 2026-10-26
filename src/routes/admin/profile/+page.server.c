import { page } from '@human-synthesis/norns/server'
import { fail } from '@sveltejs/kit'
import { auth, audit } from '$lib/norns/auth/server/public'

err := (field: string, message: string) => fail(400, { errors: [{ path: [{ key: field }], message }] })

// Every staff member's own account: details, password, two-factor, sessions.
// No capability needed beyond being signed in as staff (sessionHandle).
export load := page.load
	handler: async ({ container, url, user }) =>
		u := await auth(container).getUser user.id
		{ u, sessions: await auth(container).sessions(user.id), current: user.sessionId, enroll: url.searchParams.has('enroll') || user.needs2fa }

export actions := page.actions
	details:
		run: async ({ container, event, user }) =>
			values := Object.fromEntries(await event.request.formData()) as Record<string, string>
			r := await auth(container).updateUser user.id, { name: values.name, email: values.email, bio: values.bio, slug: values.slug, avatar: values.avatar }, user
			return fail(400, { errors: [{ path: [{ key: r.field ?? 'name' }], message: r.error }], values }) unless r.ok
			{ saved: 'Profile saved.' }
	password:
		run: async ({ container, event, user }) =>
			fd := await event.request.formData()
			next := String(fd.get('next') ?? '')
			return err('confirm', 'The new passwords do not match') unless next === String(fd.get('confirm') ?? '')
			r := await auth(container).changePassword user, String(fd.get('current') ?? ''), next
			return err((r.field === 'password' ? 'next' : r.field) ?? 'current', r.error) unless r.ok
			{ saved: 'Password changed. Other sessions were signed out.' }
	totpBegin:
		run: async ({ container, user }) =>
			setup := await auth(container).beginTotp user
			{ setup }
	totpConfirm:
		run: async ({ container, event, user }) =>
			fd := await event.request.formData()
			r := await auth(container).confirmTotp user, String(fd.get('code') ?? '')
			return fail(400, { setup: { secret: String(fd.get('secret') ?? ''), uri: String(fd.get('uri') ?? '') }, errors: [{ path: [{ key: 'code' }], message: r.error }] }) unless r.ok
			{ codes: r.codes, saved: 'Two-factor sign-in is on.' }
	totpDisable:
		run: async ({ container, event, user }) =>
			r := await auth(container).disableTotp user, String((await event.request.formData()).get('password') ?? '')
			return err('disablePassword', r.error) unless r.ok
			{ saved: 'Two-factor sign-in is off.' }
	revoke:
		run: async ({ container, event, user }) =>
			sid := String((await event.request.formData()).get('session') ?? '')
			if sid
				await auth(container).revokeSession user.id, sid
			else
				await auth(container).revokeAll user.id, user.sessionId
			await audit(container).log user, 'user.sessions_revoked', user.email, { self: true }
			{ saved: 'Signed out.' }
