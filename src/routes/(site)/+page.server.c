import { page } from '@human-synthesis/norns/server'
import { cms } from '$lib/norns/content/server/public'

export load := page.load
	handler: ({ container }) =>
		posts: cms(container).list 'post', { status: 'published', limit: 10 }
