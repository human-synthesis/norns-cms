import { page } from '@human-synthesis/norns/server'
import { fail, redirect } from '@sveltejs/kit'
import { auth, requireCap, audit, passwordProblem } from '$lib/norns/auth/server/public'

err := (field: string, message: string, values: any) => fail(400, { errors: [{ path: [{ key: field }], message }], values })

export load := page.load
	handler: async ({ container, url, user }) =>
		requireCap user, 'users.manage'
		kind := url.searchParams.get('kind') === 'client' ? 'client' : 'staff'
		{ kind, roles: await auth(container).roles(kind), isOwner: user.role === 'owner' }

export actions := page.actions
	create:
		run: async ({ container, event, user }) =>
			requireCap user, 'users.manage'
			fd := await event.request.formData()
			values := Object.fromEntries(fd) as Record<string, string>
			if values.mode === 'password'
				email := String(values.email ?? '').trim().toLowerCase()
				return err('email', 'Enter a valid email', values) unless /^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(email)
				return err('email', 'An account with that email exists', values) if await auth(container).userByEmail(email)
				role := await auth(container).role values.role
				return err('role', 'Pick a role', values) unless role
				return err('role', 'Only an owner can create owners', values) if role.id === 'owner' && user.role !== 'owner'
				problem := passwordProblem values.password, email
				return err('password', problem, values) if problem
				id := await auth(container).createUser email, values.name ?? '', values.password, role.id, { kind: role.kind, verified: true }
				await audit(container).log user, 'user.create', email, { role: role.id }
				throw redirect 303, `/admin/users/${id}?created=1`
			r := await auth(container).invite { email: values.email ?? '', name: values.name ?? '', role: values.role ?? '' }, user
			return err(r.field ?? 'email', r.error, values) unless r.ok
			{ invited: values.email, link: r.link, mailed: r.mailed, id: r.id }
