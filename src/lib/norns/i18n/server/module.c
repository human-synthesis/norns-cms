import type { Container } from '@human-synthesis/norns/server'
import { I18nRepo } from './repo'
import { I18nService } from './service'
import { registerAdminNav } from '../../content/server/public'
import { registerCapabilities } from '../../auth/server/public'
import { registerI18nApi } from './api'

export default (app: Container) =>
	app.bind 'i18n.repo', (c: Container) => new I18nRepo c.resolve('db')
	app.bind 'i18n.service', (c: Container) => new I18nService c.resolve('i18n.repo'), c.resolve('db'), c
	registerCapabilities app, [
		{ id: 'i18n.manage', label: 'Manage localisations, languages and interface texts', group: 'Localisation' }
	]
	registerAdminNav app, { id: 'i18n', label: 'Localisation', icon: 'lucide:languages', section: 'System', order: 89, capability: 'i18n.manage' }
	registerAdminNav app, { id: 'i18n-markets', parent: 'i18n', label: 'Localisations', href: '/admin/localisation', order: 1 }
	registerAdminNav app, { id: 'i18n-languages', parent: 'i18n', label: 'Languages', href: '/admin/localisation/languages', order: 2 }
	registerAdminNav app, { id: 'i18n-strings', parent: 'i18n', label: 'Interface texts', href: '/admin/localisation/texts', order: 3 }
	registerI18nApi app
