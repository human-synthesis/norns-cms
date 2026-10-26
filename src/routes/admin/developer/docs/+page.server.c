import { page } from '@human-synthesis/norns/server'
import { endpoints } from '$lib/norns/api/server/public'
import { cms, renderMarkdown } from '$lib/norns/content/server/public'
import { siteOrigin } from '$lib/norns/settings/server/public'
import guide from '../../../../../docs/API.md?raw'

// The API documentation: the guide (docs/API.md) plus a reference built from
// the live endpoint registry and this site's content types.
export load := page.load
	handler: async ({ container, user }) =>
		eps := endpoints(container).map ((ep) => ({
			method: ep.method
			path: ep.path
			capability: ep.capability ?? null
			tag: ep.tag
			summary: ep.summary
			description: ep.description ?? ''
			query: ep.query ?? []
			body: ep.body ? { description: ep.body.description ?? '', example: ep.body.example ? JSON.stringify(ep.body.example, null, 2) : '' } : null
			raw: !!ep.raw
		}))
		tags := [...new Set(eps.map((e) => e.tag))]
		types := [] as any[]
		for ct of cms(container).types()
			form := await cms(container).form ct.id
			types.push { id: ct.id, label: ct.labelPlural, fields: form.fields.map((f) => ({ name: f.name, label: f.label, type: f.type, required: !!f.required, options: f.type === 'select' ? (f.options ?? []).map((o) => o.value) : null })) }
		{
			guide: renderMarkdown(guide, {}).html
			groups: tags.map((t) => ({ tag: t, endpoints: eps.filter((e) => e.tag === t) }))
			types
			blocks: (await cms(container).form(types[0]?.id ?? 'page')).blocks.map((b) => ({ id: b.id, label: b.label, fields: b.fields.map((f) => `${f.name}${f.required ? '*' : ''} (${f.type})`) }))
			origin: siteOrigin()
			capabilities: user.capabilities ?? []
		}
