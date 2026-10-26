import type { Container } from '@human-synthesis/norns/server'
import { SeoRepo } from './repo'
import { SeoService } from './service'
import { registerFieldGroup, registerHook, registerAdminNav, registerBlock } from '../../content/server/public'
import { registerCapabilities } from '../../auth/server/public'
import { registerSettings } from '../../settings/server/public'
import { seoFieldGroup, seoSettings, locationBlock } from '../shared/fields'
import { registerSeoApi } from './api'

svc := (c: Container) => c.resolve('seo.service') as SeoService

export default (app: Container) =>
	app.bind 'seo.repo', (c: Container) => new SeoRepo c.resolve('db')
	app.bind 'seo.service', (c: Container) => new SeoService c.resolve('seo.repo'), c

	registerFieldGroup app, seoFieldGroup
	registerSettings app, seoSettings
	registerCapabilities app, [
		{ id: 'seo.manage', label: 'Manage SEO settings, redirects and the 404 log', group: 'SEO' }
	]
	registerAdminNav app, { id: 'seo', label: 'SEO', icon: 'lucide:search', href: '/admin/seo', section: 'Site', order: 70, capability: 'seo.manage' }
	registerAdminNav app, { id: 'redirects', label: 'Redirects', icon: 'lucide:corner-up-right', href: '/admin/seo/redirects', section: 'Site', order: 71, capability: 'seo.manage' }

	// A published entry's URL changed: keep the old URL working.
	registerHook app, 'slug', 'seo.redirect', ({ c, fromUrl, toUrl }) =>
		svc(c).slugChanged fromUrl, toUrl

	registerBlock app, {
		...locationBlock
		resolve: async (_props: any, { c }: any) => ({ location: await svc(c).location() })
	}
	registerSeoApi app
