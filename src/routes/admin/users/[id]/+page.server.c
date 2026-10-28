import { page } from '@human-synthesis/norns/server'
import { error, fail, redirect } from '@sveltejs/kit'
import { auth, requireCap, can, audit } from '$lib/norns/auth/server/public'
import { cms } from '$lib/norns/content/server/public'
import { twoFactor } from '$lib/norns/twofactor/server/public'

userOr404 := async (container: any, id: string) =>
	u := await auth(container).getUser Number(id)
	unless u && u.status !== 'deleted'
		throw error 404, 'User not found'
	u

export load := page.load
	handler: async ({ container, params, url, user }) =>
		requireCap user, 'users.view'
		u := await userOr404 container, params.id
		staff := u.kind === 'staff' ? await auth(container).listUsers({ kind: 'staff', status: 'active', limit: 500 }) : []
		{
			u
			twoFactor: await twoFactor(container).status(u)
			roles: await auth(container).roles(u.kind)
			sessions: await auth(container).sessions(u.id)
			activity: await audit(container).list({ actorId: u.id, limit: 15 })
			entries: u.kind === 'staff' ? await cms(container).count(null, { authorId: u.id }) : 0
			others: staff.filter((s) => s.id !== u.id).map((s) => ({ id: s.id, label: s.name || s.email }))
			canManage: can(user, 'users.manage')
			isSelf: user.id === u.id
			isOwner: user.role === 'owner'
			created: url.searchParams.has('created')
		}

fieldFail := (r: any, values: any) => fail(400, { errors: [{ path: [{ key: r.field ?? 'name' }], message: r.error }], values, error: r.field ? undefined : r.error })

export actions := page.actions
	save:
		run: async ({ container, event, user }) =>
			requireCap user, 'users.manage'
			values := Object.fromEntries(await event.request.formData()) as Record<string, string>
			r := await auth(container).updateUser Number(event.params.id), values, user
			return fieldFail(r, values) unless r.ok
			{ saved: 'Saved.' }
	reset:
		run: async ({ container, event, user }) =>
			requireCap user, 'users.manage'
			u := await userOr404 container, event.params.id
			await auth(container).requestReset u.email, 'admin'
			await audit(container).log user, 'user.reset_sent', u.email, {}
			{ saved: `A password reset link was sent to ${u.email}.` }
	resend:
		run: async ({ container, event, user }) =>
			requireCap user, 'users.manage'
			u := await userOr404 container, event.params.id
			return fail(400, { error: 'This user already accepted.' }) unless u.status === 'invited'
			// Re-inviting replaces the old token.
			await auth(container).deleteUser u.id, user
			r := await auth(container).invite { email: u.email, name: u.name, role: u.role }, user
			return fail(400, { error: r.error }) unless r.ok
			throw redirect 303, `/admin/users/${r.id}?created=1`
	revoke:
		run: async ({ container, event, user }) =>
			requireCap user, 'users.manage'
			u := await userOr404 container, event.params.id
			sid := String((await event.request.formData()).get('session') ?? '')
			if sid
				await auth(container).revokeSession u.id, sid
			else
				await auth(container).revokeAll u.id, (u.id === user.id ? user.sessionId : null)
			await audit(container).log user, 'user.sessions_revoked', u.email, { all: !sid }
			{ saved: 'Signed out.' }
	reset2fa:
		run: async ({ container, event, user }) =>
			requireCap user, 'users.manage'
			r := await twoFactor(container).reset Number(event.params.id), user
			return fail(400, { error: r.error }) unless r.ok
			{ saved: 'Two-factor sign-in was removed; they set it up again from their profile or account page.' }
	remove:
		run: async ({ container, event, user }) =>
			requireCap user, 'users.manage'
			u := await userOr404 container, event.params.id
			to := Number((await event.request.formData()).get('reassign')) || null
			r := await auth(container).deleteUser u.id, user
			return fail(400, { error: r.error }) unless r.ok
			await cms(container).reassignAuthor u.id, to
			throw redirect 303, `/admin/users?kind=${u.kind}`
