import type { Container } from '@human-synthesis/norns/server'
import { registerType, registerBlock, registerHook, registerOptions, registerAdminNav } from '../../content/server/public'
import { registerCapabilities } from '../../auth/server/public'
import { TermsRepo } from './repo'
import { ArticlesService } from './service'
import { articleType, articleListBlock, readingMinutes } from '../shared/types'
import { renderMarkdown } from '../../content/server/public'
import { registerArticlesApi } from './api'

svc := (c: Container) => c.resolve('articles.service') as ArticlesService

export default (app: Container) =>
	app.bind 'articles.repo', (c: Container) => new TermsRepo c.resolve('db')
	app.bind 'articles.service', (c: Container) => new ArticlesService c.resolve('articles.repo'), c

	registerType app, articleType
	registerCapabilities app, [
		{ id: 'taxonomy.manage', label: 'Manage categories and tags', group: 'Content' }
	]
	registerAdminNav app, { id: 'categories', parent: 'type-article', label: 'Categories', href: '/admin/taxonomy/category', order: 20, capability: 'taxonomy.manage' }
	registerAdminNav app, { id: 'tags', parent: 'type-article', label: 'Tags', href: '/admin/taxonomy/tag', order: 21, capability: 'taxonomy.manage' }

	registerOptions app, 'terms:category', (c: Container) =>
		terms := await svc(c).terms 'category'
		byId := new Map(terms.map((t) => [t.id, t]))
		depth := (t: any) =>
			d .= 0
			p .= t
			while p?.parent_id && d < 5
				p = byId.get p.parent_id
				d++
			d
		terms.map (t) => ({ value: t.id, label: `${'— '.repeat(depth(t))}${t.name}` })

	// Computed fields and defaults: reading time, word count, authors.
	registerHook app, 'beforeSave', 'articles.computed', ({ ct, data, user }) =>
		return unless ct.id === 'article'
		words := renderMarkdown(String(data.body ?? ''), {}).words
		authors := data.authors?.length ? data.authors : (user?.id ? [user.id] : [])
		{ ...data, words, readingTime: readingMinutes(words), authors }

	registerHook app, 'afterSave', 'articles.terms', ({ c, ct, entry }) =>
		await svc(c).syncEntry(entry) if ct.id === 'article'

	registerHook app, 'status', 'articles.review', ({ c, entry, from, to, user }) =>
		await svc(c).notifyReview(entry, user) if to === 'in_review' && from !== 'in_review'

	registerBlock app, {
		...articleListBlock
		resolve: async (props: any, { c, viewer }: any) =>
			s := svc(c)
			count := Math.min(24, Math.max(1, Number(props.count) || 3))
			termId .= undefined as number | undefined
			if props.source === 'category' && props.category
				termId = Number(props.category)
			else if props.source === 'tag' && props.tag
				t := await s.termBySlug 'tag', String(props.tag).toLowerCase().replace(/[^a-z0-9]+/g, '-')
				termId = t?.id ?? -1
			list := await s.list { perPage: count, termId, featured: props.source === 'featured', member: !!viewer?.member }
			{ entries: list.entries, moreHref: termId && termId > 0 ? null : '/articles' }
	}
	registerArticlesApi app
