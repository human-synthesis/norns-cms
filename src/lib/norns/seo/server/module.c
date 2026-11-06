import type { Container } from '@human-synthesis/norns/server'
import { SeoRepo } from './repo'
import { SeoService } from './service'
import { IndexingService } from './indexing'
import { registerFieldGroup, registerHook, registerAdminNav, registerBlock } from '../../content/server/public'
import { registerCapabilities } from '../../auth/server/public'
import { registerSettings } from '../../settings/server/public'
import { seoFieldGroup, seoSettings, locationBlock } from '../shared/fields'
import { registerSeoApi } from './api'

svc := (c: Container) => c.resolve('seo.service') as SeoService
ix := (c: Container) => c.resolve('seo.indexing') as IndexingService

export default (app: Container) =>
	app.bind 'seo.repo', (c: Container) => new SeoRepo c.resolve('db')
	app.bind 'seo.service', (c: Container) => new SeoService c.resolve('seo.repo'), c
	app.bind 'seo.indexing', (c: Container) => new IndexingService c.resolve('seo.repo'), c

	registerFieldGroup app, seoFieldGroup
	registerSettings app, seoSettings
	registerCapabilities app, [
		{ id: 'seo.manage', label: 'Manage SEO settings, redirects and the 404 log', group: 'SEO' }
	]
	registerAdminNav app, { id: 'seo', label: 'SEO & GEO', icon: 'lucide:search', section: 'Marketing', order: 70, capability: 'seo.manage' }
	registerAdminNav app, { id: 'seo-overview', parent: 'seo', label: 'Report & 404s', href: '/admin/seo', order: 1 }
	registerAdminNav app, { id: 'seo-redirects', parent: 'seo', label: 'Redirects', href: '/admin/seo/redirects', order: 2 }
	registerAdminNav app, { id: 'seo-settings', parent: 'seo', label: 'Settings', href: '/admin/settings/seo', order: 3 }

	// A published entry's URL changed: keep the old URL working.
	registerHook app, 'slug', 'seo.redirect', ({ c, fromUrl, toUrl }) =>
		svc(c).slugChanged fromUrl, toUrl

	// Tell search engines about new, updated, moved and removed URLs (the
	// IndexNow rules decide which go out automatically).
	registerHook app, 'afterSave', 'seo.indexing', ({ c, ct, entry, prev }) =>
		ix(c).changed ct, { now: entry, kind: 'updated', gone: (prev?.status === 'published' && prev.slug !== entry.slug ? prev : null) }
	registerHook app, 'status', 'seo.indexing', ({ c, ct, entry, from, to }) =>
		if to === 'published'
			ix(c).changed ct, { now: entry, kind: (from === 'published' ? 'updated' : 'new') }
		else if from === 'published'
			ix(c).changed ct, { gone: entry }
	registerHook app, 'delete', 'seo.indexing', ({ c, ct, entry }) =>
		ix(c).changed(ct, { gone: entry }) if entry.status === 'published'

	registerBlock app, {
		...locationBlock
		resolve: async (_props: any, { c }: any) => ({ location: await svc(c).location() })
	}
	registerSeoApi app
