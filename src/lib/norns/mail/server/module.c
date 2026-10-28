import type { Container } from '@human-synthesis/norns/server'
import { MailService } from './service'
import { registerAdminNav } from '../../content/server/public'
import { registerSettings, settings, envVar, siteOrigin } from '../../settings/server/public'
import { registerMailApi } from './api'
import { mailSettings } from '../shared/settings'

// Mail domain from Settings → Email, else MAIL_DOMAIN, else the site's own
// host without www; a full address set per purpose wins over the domain.
// Sender name falls back to MAIL_FROM_NAME, then the site title.
senderFor := (c: Container) => async =>
	cfg := await settings(c).get 'mail'
	site := await settings(c).get 'site'
	{
		domain: String(cfg.domain || envVar('MAIL_DOMAIN') || new URL(siteOrigin()).hostname.replace(/^www\./, '')).trim().replace(/^@/, '').toLowerCase()
		addresses: { signup: cfg.signupFrom, signin: cfg.signinFrom, notifications: cfg.notificationsFrom, contact: cfg.contactFrom }
		fromName: cfg.fromName || envVar('MAIL_FROM_NAME') || site.title
		replyTo: cfg.replyTo || ''
	}

export default (app: Container) =>
	app.bind 'mail.service', (c: Container) => new MailService c.resolve('db'), senderFor(c)
	registerSettings app, mailSettings
	registerAdminNav app, { id: 'logs-mail', parent: 'logs', label: 'Mail log', href: '/admin/mail', order: 2, capability: 'settings.manage' }
	registerAdminNav app, { id: 'settings-mail', parent: 'settings', label: 'Email', href: '/admin/settings/mail', order: 3 }
	registerMailApi app
