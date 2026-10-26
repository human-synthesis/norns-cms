import type { Container } from '@human-synthesis/norns/server'
import type { ArticlesService } from './service'
import { TAXONOMIES } from './service'

export { TAXONOMIES }

svc := (c: Container) => c.resolve('articles.service') as ArticlesService

export articles := (c: Container) => {
	taxonomy: (id: string) => svc(c).taxonomy(id)
	terms: (taxonomy: string) => svc(c).terms(taxonomy)
	term: (id: number) => svc(c).term(id)
	termBySlug: (taxonomy: string, slug: string) => svc(c).termBySlug(taxonomy, slug)
	termUrl: (t: any) => svc(c).termUrl(t)
	saveTerm: (taxonomy: string, id: number | null, input: any) => svc(c).saveTerm(taxonomy, id, input)
	deleteTerm: (id: number) => svc(c).deleteTerm(id)
	mergeTerm: (fromId: number, intoId: number, user: any) => svc(c).mergeTerm(fromId, intoId, user)
	list: (opts?: any) => svc(c).list(opts)
	byAuthor: (authorId: number, opts?: any) => svc(c).byAuthor(authorId, opts)
	related: (entry: any, limit?: number) => svc(c).related(entry, limit)
	decorate: (entries: any[]) => svc(c).decorate(entries)
	sources: (entry: any) => svc(c).sources(entry)
}
