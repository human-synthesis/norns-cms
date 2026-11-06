import type { Container } from '@human-synthesis/norns/server'
import { ProtectionRepo } from './repo'
import { ProtectionService } from './service'
import { BlockedPaths } from './paths'
import { CountryFilter } from './countries'
import { CloudflareFirewall } from './cloudflare'
import { registerCapabilities } from '../../auth/server/public'
import { registerAdminNav } from '../../content/server/public'
import { registerProtectionApi } from './api'

export default (app: Container) =>
	app.bind 'protection.repo', (c: Container) => new ProtectionRepo c.resolve('db')
	app.bind 'protection.service', (c: Container) => new ProtectionService c.resolve('protection.repo'), c
	app.bind 'protection.paths', (c: Container) => new BlockedPaths c.resolve('protection.repo'), c
	app.bind 'protection.countries', (c: Container) => new CountryFilter c.resolve('protection.repo'), c
	app.bind 'protection.cloudflare', (c: Container) => new CloudflareFirewall c.resolve('protection.repo'), c
	registerCapabilities app, [
		{ id: 'protection.manage', label: 'Security: form protection, blocked paths, country filter', group: 'Admin' }
	]
	// Security: one panel for everything that keeps unwanted traffic out.
	registerAdminNav app, { id: 'security', label: 'Security', icon: 'lucide:shield', section: 'System', order: 91 }
	registerAdminNav app, { id: 'security-overview', parent: 'security', label: 'Overview', href: '/admin/security', order: 1, capability: 'protection.manage' }
	registerAdminNav app, { id: 'security-countries', parent: 'security', label: 'Country filter', href: '/admin/security/countries', order: 2, capability: 'protection.manage' }
	registerAdminNav app, { id: 'security-paths', parent: 'security', label: 'Blocked paths', href: '/admin/security/paths', order: 3, capability: 'protection.manage' }
	registerAdminNav app, { id: 'security-cloudflare', parent: 'security', label: 'Cloudflare firewall', href: '/admin/security/cloudflare', order: 4, capability: 'protection.manage' }
	registerAdminNav app, { id: 'security-forms', parent: 'security', label: 'Form protection', href: '/admin/protection', order: 5, capability: 'protection.manage' }
	registerAdminNav app, { id: 'security-headers', parent: 'security', label: 'Headers & CSP', href: '/admin/settings/security', order: 6, capability: 'settings.manage' }
	registerAdminNav app, { id: 'forms-protection', parent: 'forms', label: 'Spam protection', href: '/admin/protection', order: 4, capability: 'protection.manage' }
	registerProtectionApi app
