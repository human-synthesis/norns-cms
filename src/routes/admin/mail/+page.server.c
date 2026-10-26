import { page } from '@human-synthesis/norns/server'
import { mail } from '$lib/norns/mail/server/public'
import { requireCap } from '$lib/norns/auth/server/public'

export load := page.load
	handler: async ({ container, user }) =>
		requireCap user, 'settings.manage'
		// Logged bodies hold invite and reset links (log provider only), so
		// only owners see them; anyone else could take over another account.
		log := await mail(container).log 100
		shown := user.role === 'owner' ? log : log.map((m) => ({ ...m, body: m.body ? '(only owners can read logged message bodies)' : '' }))
		{ log: shown, provider: mail(container).provider() }
