import type { Container } from '@human-synthesis/norns/server'
import { ConsentRepo } from './repo'
import { ConsentService } from './service'
import { registerAdminNav, registerBlock } from '../../content/server/public'
import { registerCapabilities } from '../../auth/server/public'
import { registerSettings } from '../../settings/server/public'
import { consentSettings, cookieTableBlock } from '../shared/config'

export default (app: Container) =>
	app.bind 'consent.repo', (c: Container) => new ConsentRepo c.resolve('db')
	app.bind 'consent.service', (c: Container) => new ConsentService c.resolve('consent.repo'), c.resolve('db'), c

	registerSettings app, consentSettings
	registerCapabilities app, [
		{ id: 'consent.view', label: 'See consent statistics and records', group: 'Consent' }
		{ id: 'consent.manage', label: 'Change banner settings and preset scripts', group: 'Consent' }
		{ id: 'consent.scripts_custom', label: 'Add custom code to the public site', group: 'Consent' }
	]
	registerAdminNav app, { id: 'consent', label: 'Consent', icon: 'lucide:cookie', href: '/admin/consent', section: 'People', order: 60, capability: 'consent.view' }
	registerBlock app, {
		...cookieTableBlock
		resolve: async (_props: any, { c }: any) => ({ declaration: await (c.resolve('consent.service') as ConsentService).declaration() })
	}
