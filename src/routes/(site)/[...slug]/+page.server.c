import { page } from '@human-synthesis/norns/server'
import { error } from '@sveltejs/kit'
import { cms } from '$lib/norns/content/server/public'

// Logged-in users see drafts (preview); everyone else only published entries.
export load := page.load
	handler: ({ container, params, user }) =>
		res := cms(container).resolve params.slug, !!user
		unless res
			throw error 404, 'Page not found'
		res
