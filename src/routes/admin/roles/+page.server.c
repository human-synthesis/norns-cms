import { page } from '@human-synthesis/norns/server'
import { auth, requireCap } from '$lib/norns/auth/server/public'

export load := page.load
	handler: async ({ container, user }) =>
		requireCap user, 'roles.manage'
		roles := await auth(container).roles()
		out := [] as any[]
		for r of roles
			out.push { ...r, users: await auth(container).countUsers({ role: r.id }) }
		{ roles: out }
