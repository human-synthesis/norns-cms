import type { Container } from '@human-synthesis/norns/server'
import { MailService } from './service'
import { registerAdminNav } from '../../content/server/public'
import { registerSettings, settings, envVar } from '../../settings/server/public'
import { registerMailApi } from './api'
import { mailSettings } from '../shared/settings'

// Sender from Settings → Email (MAIL_FROM_* env vars as fallbacks).
senderFor := (c: Container) => async =>
	cfg := await settings(c).get 'mail'
	site := await settings(c).get 'site'
	{
		fromEmail: cfg.fromEmail || envVar('MAIL_FROM_EMAIL') || 'no-reply@humansynthesis.ai'
		fromName: cfg.fromName || envVar('MAIL_FROM_NAME') || site.title
		replyTo: cfg.replyTo || ''
	}

export default (app: Container) =>
	app.bind 'mail.service', (c: Container) => new MailService c.resolve('db'), senderFor(c)
	registerSettings app, mailSettings
	registerAdminNav app, { id: 'logs-mail', parent: 'logs', label: 'Mail log', href: '/admin/mail', order: 2, capability: 'settings.manage' }
	registerAdminNav app, { id: 'settings-mail', parent: 'settings', label: 'Email', href: '/admin/settings/mail', order: 3 }
	registerMailApi app
