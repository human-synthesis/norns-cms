import { page } from '@human-synthesis/norns/server'
import { i18n } from '$lib/norns/i18n/server/public'
import { requireCap } from '$lib/norns/auth/server/public'

export load := page.load
	handler: async ({ container, user }) =>
		requireCap user, 'i18n.manage'
		{ markets: await i18n(container).map(), languages: await i18n(container).allLanguages() }
