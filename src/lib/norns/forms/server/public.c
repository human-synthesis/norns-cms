import type { Container } from '@human-synthesis/norns/server'
import type { FormsService } from './service'

export { localizeForm, describeSubmission, FIELD_TYPES } from '../shared/forms.js'

svc := (c: Container) => c.resolve('forms.service') as FormsService

export forms := (c: Container) => {
	list: () => svc(c).list()
	get: (id: number) => svc(c).get(id)
	bySlug: (slug: string) => svc(c).bySlug(slug)
	localized: (slug: string, lang: string) => svc(c).localized(slug, lang)
	save: (id: number | null, input: any, user?: any) => svc(c).save(id, input, user)
	remove: (id: number) => svc(c).remove(id)
	submit: (slug: string, raw: Record<string, any>, ctx: Record<string, any>) => svc(c).submit(slug, raw, ctx)
	submissions: (q?: any) => svc(c).submissions(q)
	count: (q?: any) => svc(c).count(q)
	submission: (id: number) => svc(c).submission(id)
	setStatus: (ids: number[], status: string) => svc(c).setStatus(ids, status)
	removeSubmissions: (ids: number[]) => svc(c).removeSubmissions(ids)
	csv: (formId: number) => svc(c).csv(formId)
	purge: () => svc(c).purge()
}
