import { page } from '@human-synthesis/norns/server'
import { runMaintenance } from '$lib/norns/settings/server/public'
import { cms } from '$lib/norns/content/server/public'
import { requireCap, audit } from '$lib/norns/auth/server/public'

export load := page.load
	handler: ({ user }) =>
		requireCap user, 'maintenance.run'
		{}

export actions := page.actions
	purge:
		run: async ({ container, user }) =>
			requireCap user, 'maintenance.run'
			result := await runMaintenance container
			await audit(container).log user, 'maintenance.purge', '', result
			{ result }
	rerender:
		run: async ({ container, user }) =>
			requireCap user, 'maintenance.run'
			n := await cms(container).rerender()
			await audit(container).log user, 'maintenance.rerender', `${n} entries`, {}
			{ rerendered: n }
