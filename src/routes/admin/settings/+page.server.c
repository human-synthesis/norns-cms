import { page } from '@human-synthesis/norns/server'
import { redirect } from '@sveltejs/kit'
import { settings } from '$lib/norns/settings/server/public'
import { requireCap } from '$lib/norns/auth/server/public'

export load := page.load
	handler: async ({ container, user }) =>
		requireCap user, 'settings.manage'
		groups := settings(container).groups().map((g) => ({ id: g.id, label: g.label, icon: g.icon, description: g.description }))
		{ groups }
