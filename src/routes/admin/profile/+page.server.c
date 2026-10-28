import { page } from '@human-synthesis/norns/server'
import { fail } from '@sveltejs/kit'
import { auth, audit } from '$lib/norns/auth/server/public'
import { twoFactor, twoFactorActions } from '$lib/norns/twofactor/server/public'
import { translate } from '$lib/norns/i18n/shared/t.js'
import { en } from '$lib/norns/i18n/shared/catalog.js'

err := (field: string, message: string) => fail(400, { errors: [{ path: [{ key: field }], message }] })

// Every staff member's own account: details, password, two-factor, sessions.
// No capability needed beyond being signed in as staff (sessionHandle).
export load := page.load
	handler: async ({ container, user }) =>
		u := await auth(container).getUser user.id
		{ u, twoFactor: await twoFactor(container).status(user), setup: await twoFactor(container).pending(user), sessions: await auth(container).sessions(user.id), current: user.sessionId }

export actions := page.actions {
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
	...twoFactorActions(async () => (key: string, vars?: any) => translate(en, key, vars))
	revoke:
		run: async ({ container, event, user }) =>
			sid := String((await event.request.formData()).get('session') ?? '')
			if sid
				await auth(container).revokeSession user.id, sid
			else
				await auth(container).revokeAll user.id, user.sessionId
			await audit(container).log user, 'user.sessions_revoked', user.email, { self: true }
			{ saved: 'Signed out.' }
}
