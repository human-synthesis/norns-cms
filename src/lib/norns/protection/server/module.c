import type { Container } from '@human-synthesis/norns/server'
import { ProtectionRepo } from './repo'
import { ProtectionService } from './service'
import { registerCapabilities } from '../../auth/server/public'
import { registerAdminNav } from '../../content/server/public'
import { registerProtectionApi } from './api'

export default (app: Container) =>
	app.bind 'protection.repo', (c: Container) => new ProtectionRepo c.resolve('db')
	app.bind 'protection.service', (c: Container) => new ProtectionService c.resolve('protection.repo'), c
	registerCapabilities app, [
		{ id: 'protection.manage', label: 'Set Turnstile keys and form protection', group: 'Admin' }
	]
	registerAdminNav app, { id: 'protection', label: 'Form protection', icon: 'lucide:shield-check', href: '/admin/protection', section: 'System', order: 92, capability: 'protection.manage' }
	registerAdminNav app, { id: 'forms-protection', parent: 'forms', label: 'Spam protection', href: '/admin/protection', order: 4, capability: 'protection.manage' }
	registerProtectionApi app
