import type { Container } from '@human-synthesis/norns/server'
import type { ContentService } from './service'
import type { ContentType } from '../shared/types'
import type { Entry, ListOpts } from './repo'

export type { ContentType, Entry }

svc := (c: Container) => c.resolve('content.service') as ContentService

// The content facade: routes and other features call this, never the
// service or the repo directly.
export cms := (c: Container) => {
	types: () => svc(c).types()
	getType: (id: string) => svc(c).getType(id)
	form: (typeId: string) => svc(c).form(typeId)
	list: (typeId: string, opts?: ListOpts) => svc(c).list(typeId, opts)
	count: (typeId: string, status?: string) => svc(c).count(typeId, status)
	get: (typeId: string, id: number) => svc(c).get(typeId, id)
	recent: (limit?: number) => svc(c).recent(limit)
	save: (typeId: string, id: number | null, raw: Record<string, any>, userId: number | null) => svc(c).save(typeId, id, raw, userId)
	setStatus: (typeId: string, id: number, status: string) => svc(c).setStatus(typeId, id, status)
	remove: (typeId: string, id: number) => svc(c).remove(typeId, id)
	revisions: (entryId: number) => svc(c).revisions(entryId)
	render: (ct: ContentType, entry: Entry) => svc(c).render(ct, entry)
	urlFor: (ct: ContentType, entry: Entry) => svc(c).urlFor(ct, entry)
	resolve: (path: string, preview?: boolean) => svc(c).resolve(path, preview)
	nav: () => svc(c).nav()
	adminNav: () => svc(c).adminNav()
	counts: () => svc(c).counts()
}

// Called from a feature's module.c to contribute a content type. Types are
// bindings on the container, so registration order never matters.
export registerType := (app: Container, ct: ContentType) =>
	app.single `cms.type:${ct.id}`, => ct
