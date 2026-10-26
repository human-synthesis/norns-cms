import { route } from '@human-synthesis/norns/server'
import { json } from '@sveltejs/kit'
import { cms, renderMarkdown, collectRefs } from '$lib/norns/content/server/public'
import { requireCap } from '$lib/norns/auth/server/public'

// POST { markdown } → { html, toc, words }: the editor's preview, rendered by
// the same renderer the public site uses (media resolved, sanitised).
export POST := route
	handler: async ({ container, event, user }) =>
		requireCap user, 'content.view'
		body := await event.request.json().catch(=> ({}))
		md := String(body.markdown ?? '').slice(0, 500000)
		refs := collectRefs md
		mediaMap := await cms(container).lookupMedia refs.media
		out := renderMarkdown md, { media: mediaMap }
		json out
