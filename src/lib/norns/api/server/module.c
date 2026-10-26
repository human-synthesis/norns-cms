import type { Container } from '@human-synthesis/norns/server'
import { ApiKeysRepo } from './repo'
import { ApiKeysService } from './service'
import { registerAdminNav } from '../../content/server/public'
import { registerCapabilities } from '../../auth/server/public'
import { registerEndpoint } from './public'

export default (app: Container) =>
	app.bind 'api.keys.repo', (c: Container) => new ApiKeysRepo c.resolve('db')
	app.bind 'api.keys', (c: Container) => new ApiKeysService c.resolve('api.keys.repo'), c
	registerCapabilities app, [
		{ id: 'api.manage', label: 'Create and revoke API keys', group: 'API' }
	]
	registerAdminNav app, { id: 'api', label: 'API', icon: 'lucide:braces', section: 'System', order: 93, capability: 'admin.access' }
	registerAdminNav app, { id: 'api-keys', parent: 'api', label: 'API keys', href: '/admin/developer', order: 1 }
	registerAdminNav app, { id: 'api-docs', parent: 'api', label: 'Documentation', href: '/admin/developer/docs', order: 2 }

	registerEndpoint app, {
		method: 'GET', path: '/me', capability: null, tag: 'Account'
		summary: 'The key and the user it acts for, with its effective capabilities'
		handler: ({ user }) => ({ key: user.apiKey, user: { id: user.id, email: user.userEmail, name: user.name, role: user.role }, capabilities: user.capabilities })
	}
