import type { Container } from '@human-synthesis/norns/server'
import { compileFields } from '../../content/server/public'
import { SettingsRegistry } from './registry'
import { SettingsService } from './service'
import { registerSettings } from './public'
import { registerAdminNav } from '../../content/server/public'
import { registerSettingsApi } from './api'
import { siteSettings, securitySettings } from '../shared/groups'

export default (app: Container) =>
	app.single 'settings.registry', (c: Container) => new SettingsRegistry c
	app.bind 'settings.service', (c: Container) => new SettingsService c.resolve('settings.registry'), c.resolve('db'), compileFields
	registerSettings app, siteSettings
	registerSettings app, securitySettings
	registerAdminNav app, { id: 'settings', label: 'Settings', icon: 'lucide:settings', section: 'System', order: 90, capability: 'settings.manage' }
	registerAdminNav app, { id: 'settings-site', parent: 'settings', label: 'Site', href: '/admin/settings/site', order: 1 }
	registerAdminNav app, { id: 'settings-security', parent: 'settings', label: 'Security', href: '/admin/settings/security', order: 2 }
	registerAdminNav app, { id: 'settings-all', parent: 'settings', label: 'All settings', href: '/admin/settings', order: 9 }
	registerAdminNav app, { id: 'maintenance', label: 'Maintenance', icon: 'lucide:wrench', href: '/admin/maintenance', section: 'System', order: 97, capability: 'maintenance.run' }
	registerSettingsApi app
