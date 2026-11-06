import type { Container } from '@human-synthesis/norns/server'
import type { CommentsService } from './service'

export { commentHtml } from '../shared/settings'

svc := (c: Container) => c.resolve('comments.service') as CommentsService

// The comments facade: the theme, the posting endpoint, the admin and the
// API call this.
export comments := (c: Container) => {
	config: () => svc(c).config()
	modeFor: (ct: any, entry: any) => svc(c).modeFor(ct, entry)
	section: (ct: any, entry: any, viewer?: { user?: any }) => svc(c).section(ct, entry, viewer)
	approved: (entryId: number, limit?: number) => svc(c).approved(entryId, limit)
	post: (input: Record<string, any>, ctx?: Record<string, any>) => svc(c).post(input, ctx)
	get: (id: number) => svc(c).get(id)
	list: (opts?: Record<string, any>) => svc(c).list(opts)
	setStatus: (id: number, status: string, user?: any) => svc(c).setStatus(id, status, user)
	edit: (id: number, body: string) => svc(c).edit(id, body)
	remove: (id: number) => svc(c).remove(id)
	reply: (id: number, body: string, user: any) => svc(c).reply(id, body, user)
	counts: () => svc(c).counts()
	pending: () => svc(c).pending()
	byUser: (userId: number) => svc(c).byUser(userId)
	purge: () => svc(c).purge()
}
