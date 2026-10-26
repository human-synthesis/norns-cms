import type { Container } from '@human-synthesis/norns/server'
import { MediaService, MAX_BYTES, ALLOWED } from './service'
import type { MediaRepo } from './repo'
import { settings } from '../../settings/server/public'

export { MAX_BYTES, ALLOWED }

// The service is built per call because Cloudflare Image Resizing is a site
// setting (cached by the settings service, so this is cheap).
svc := async (c: Container) =>
	site := await settings(c).get 'site'
	new MediaService c.resolve('media.repo') as MediaRepo, !!site.imageResizing

export media := (c: Container) => {
	upload: async (file: File, meta?: any) => (await svc(c)).upload(file, meta)
	read: async (key: string) => (await svc(c)).read(key)
	get: async (id: number) => (await svc(c)).get(id)
	resolve: async (ids: number[]) => (await svc(c)).resolve(ids)
	list: async (q?: any) => (await svc(c)).list(q)
	count: async () => (await svc(c)).count()
	update: async (id: number, fields: any) => (await svc(c)).update(id, fields)
	usage: async (id: number) => (await svc(c)).usage(id)
	remove: async (id: number, force?: boolean) => (await svc(c)).remove(id, force)
	setUsage: async (entryId: number, ids: number[]) => (await svc(c)).setUsage(entryId, ids)
}
