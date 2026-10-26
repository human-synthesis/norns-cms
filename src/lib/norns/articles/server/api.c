import type { Container } from '@human-synthesis/norns/server'
import { registerEndpoint, apiError, need } from '../../api/server/public'
import { articles, TAXONOMIES } from './public'
import { seo } from '../../seo/server/public'
import { audit } from '../../auth/server/public'

taxOf := (c: Container, id: string) => need articles(c).taxonomy(id), `Unknown taxonomy "${id}"`

termOf := async (c: Container, tax: any, id: string) =>
	t := await articles(c).term Number(id)
	throw apiError(404, 'Term not found') unless t && t.taxonomy === tax.id
	t

withUrl := (c: Container, t: any) => ({ ...t, url: articles(c).termUrl(t) })

export registerArticlesApi := (app: Container) =>
	registerEndpoint app, {
		method: 'GET', path: '/taxonomies', capability: 'content.view', tag: 'Taxonomies'
		summary: 'The taxonomies (category, tag)'
		handler: => Object.values(TAXONOMIES)
	}
	registerEndpoint app, {
		method: 'GET', path: '/taxonomies/:taxonomy/terms', capability: 'content.view', tag: 'Taxonomies'
		summary: 'Terms with their published article counts'
		handler: async ({ c, params }) =>
			tax := taxOf c, params.taxonomy
			(await articles(c).terms tax.id).map((t) => withUrl(c, t))
	}
	registerEndpoint app, {
		method: 'POST', path: '/taxonomies/:taxonomy/terms', capability: 'taxonomy.manage', tag: 'Taxonomies', created: true
		summary: 'Create a term'
		body: { description: 'name, slug?, description?, parent_id? (categories), sort?', example: { name: 'Guides', description: 'Step by step' } }
		handler: async ({ c, params, body, user }) =>
			tax := taxOf c, params.taxonomy
			r := await articles(c).saveTerm tax.id, null, body
			throw apiError(400, r.error, { field: r.field }) unless r.ok
			await audit(c).log user, 'taxonomy.create', `${tax.id}:${r.term.slug}`, { via: 'api' }
			withUrl c, r.term
	}
	registerEndpoint app, {
		method: 'PATCH', path: '/taxonomies/:taxonomy/terms/:id', capability: 'taxonomy.manage', tag: 'Taxonomies'
		summary: 'Update a term (a new slug keeps the old archive URL with a redirect)'
		body: { description: 'name, slug, description, parent_id, sort', example: { slug: 'how-to' } }
		handler: async ({ c, params, body, user }) =>
			tax := taxOf c, params.taxonomy
			t := await termOf c, tax, params.id
			r := await articles(c).saveTerm tax.id, t.id, { ...t, ...body }
			throw apiError(400, r.error, { field: r.field }) unless r.ok
			if r.term.slug !== t.slug
				await seo(c).saveRedirect null, { from: articles(c).termUrl(t), to: articles(c).termUrl(r.term), status: 301 }
			await audit(c).log user, 'taxonomy.update', `${tax.id}:${r.term.slug}`, { via: 'api' }
			withUrl c, r.term
	}
	registerEndpoint app, {
		method: 'DELETE', path: '/taxonomies/:taxonomy/terms/:id', capability: 'taxonomy.manage', tag: 'Taxonomies'
		summary: 'Delete a term'
		handler: async ({ c, params, user }) =>
			tax := taxOf c, params.taxonomy
			t := await termOf c, tax, params.id
			await articles(c).deleteTerm t.id
			await audit(c).log user, 'taxonomy.delete', `${tax.id}:${t.slug}`, { via: 'api' }
			{ deleted: t.id }
	}
	registerEndpoint app, {
		method: 'POST', path: '/taxonomies/:taxonomy/terms/:id/merge', capability: 'taxonomy.manage', tag: 'Taxonomies'
		summary: 'Merge a term into another: its articles move over, it is removed'
		body: { description: 'into: the id of the term to keep', example: { into: 4 } }
		handler: async ({ c, params, body, user }) =>
			tax := taxOf c, params.taxonomy
			t := await termOf c, tax, params.id
			r := await articles(c).mergeTerm t.id, Number(body.into), user
			throw apiError(400, r.error) unless r.ok
			await seo(c).saveRedirect null, { from: articles(c).termUrl(t), to: articles(c).termUrl(r.into), status: 301 }
			await audit(c).log user, 'taxonomy.merge', `${tax.id}:${t.slug}`, { into: r.into.slug, via: 'api' }
			withUrl c, r.into
	}
