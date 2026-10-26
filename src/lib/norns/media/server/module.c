import type { Container } from '@human-synthesis/norns/server'
import { MediaRepo } from './repo'
import { registerAdminNav, registerHook, cms } from '../../content/server/public'
import { registerCapabilities } from '../../auth/server/public'
import { media } from './public'
import { registerMediaApi } from './api'

export default (app: Container) =>
	app.bind 'media.repo', (c: Container) => new MediaRepo c.resolve('db')
	// The content engine looks this token up to turn media ids into URLs,
	// dimensions and alt text (markdown figures, image fields, blocks).
	app.bind 'content.mediaResolver', (c: Container) => (ids: number[]) => media(c).resolve(ids)

	registerCapabilities app, [
		{ id: 'media.view', label: 'Browse the media library', group: 'Media' }
		{ id: 'media.upload', label: 'Upload media', group: 'Media' }
		{ id: 'media.edit', label: 'Edit alt text and captions', group: 'Media' }
		{ id: 'media.delete', label: 'Delete media', group: 'Media' }
	]
	registerAdminNav app, { id: 'media', label: 'Media', icon: 'lucide:image', href: '/admin/media', section: 'Content', order: 30, capability: 'media.view' }

	// Usage tracking: which entries reference which media (image fields and
	// media: links in markdown). Guards deletes and drives re-rendering
	// when alt text changes.
	registerHook app, 'afterSave', 'media.usage', ({ c, ct, entry }) =>
		media(c).setUsage entry.id, cms(c).mediaRefs(ct, entry.data)
	registerMediaApi app
