import { page } from '@human-synthesis/norns/server'
import { auth, requireCap, can } from '$lib/norns/auth/server/public'

export load := page.load
	handler: async ({ container, url, user }) =>
		requireCap user, 'users.view'
		kind := url.searchParams.get('kind') === 'client' ? 'client' : 'staff'
		q := url.searchParams.get('q') ?? ''
		role := url.searchParams.get('role') ?? ''
		status := url.searchParams.get('status') ?? ''
		users := await auth(container).listUsers { kind, q: q || undefined, role: role || undefined, status: status || undefined, limit: 500 }
		{
			kind, q, role, status
			users
			roles: await auth(container).roles(kind)
			counts: { staff: await auth(container).countUsers({ kind: 'staff' }), client: await auth(container).countUsers({ kind: 'client' }) }
			canManage: can(user, 'users.manage')
		}
