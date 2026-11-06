import { route } from '@human-synthesis/norns/server'
import { error } from '@sveltejs/kit'
import { auth } from '$lib/norns/auth/server/public'
import { consent } from '$lib/norns/consent/server/public'
import { comments } from '$lib/norns/comments/server/public'

// The member's own data (GDPR access / portability).
export GET := route
	handler: async ({ container, user }) =>
		unless user
			throw error 403, 'Sign in first'
		data := await auth(container).exportData user.id
		body := { ...data, consent: await consent(container).records({ userId: user.id, limit: 1000 }), comments: await comments(container).byUser(user.id) }
		new Response(JSON.stringify(body, null, 2), { headers: { 'content-type': 'application/json', 'content-disposition': 'attachment; filename="my-data.json"', 'cache-control': 'private, no-store' } })
