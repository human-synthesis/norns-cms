import type { Container } from '@human-synthesis/norns/server'
import { ConsentRepo } from './repo'
import { ConsentService } from './service'
import { registerAdminNav, registerBlock } from '../../content/server/public'
import { registerCapabilities } from '../../auth/server/public'
import { registerSettings } from '../../settings/server/public'
import { consentSettings, cookieTableBlock } from '../shared/config'
import { registerConsentApi } from './api'

export default (app: Container) =>
	app.bind 'consent.repo', (c: Container) => new ConsentRepo c.resolve('db')
	app.bind 'consent.service', (c: Container) => new ConsentService c.resolve('consent.repo'), c.resolve('db'), c

	registerSettings app, consentSettings
	registerCapabilities app, [
		{ id: 'consent.view', label: 'See consent statistics and records', group: 'Consent' }
		{ id: 'consent.manage', label: 'Change banner settings and preset scripts', group: 'Consent' }
		{ id: 'consent.scripts_custom', label: 'Add custom code to the public site', group: 'Consent' }
	]
	registerAdminNav app, { id: 'consent', label: 'Cookie consent', icon: 'lucide:cookie', section: 'Marketing', order: 75, capability: 'consent.view' }
	registerAdminNav app, { id: 'consent-overview', parent: 'consent', label: 'Overview', href: '/admin/consent', order: 1 }
	registerAdminNav app, { id: 'consent-scripts', parent: 'consent', label: 'Scripts & analytics', href: '/admin/consent/scripts', order: 2, capability: 'consent.manage' }
	registerAdminNav app, { id: 'consent-log', parent: 'consent', label: 'Consent log', href: '/admin/consent/log', order: 3 }
	registerAdminNav app, { id: 'consent-settings', parent: 'consent', label: 'Banner & rules', href: '/admin/settings/consent', order: 4, capability: 'consent.manage' }
	registerBlock app, {
		...cookieTableBlock
		resolve: async (_props: any, { c }: any) => ({ declaration: await (c.resolve('consent.service') as ConsentService).declaration() })
	}
	registerConsentApi app
