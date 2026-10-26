import type { Container } from '@human-synthesis/norns/server'
import { MailService } from './service'
import { registerAdminNav } from '../../content/server/public'
import { registerMailApi } from './api'

export default (app: Container) =>
	app.bind 'mail.service', (c: Container) => new MailService c.resolve('db')
	registerAdminNav app, { id: 'logs-mail', parent: 'logs', label: 'Mail log', href: '/admin/mail', order: 2, capability: 'settings.manage' }
	registerMailApi app
