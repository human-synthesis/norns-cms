import { page } from '@human-synthesis/norns/server'
import { redirect } from '@sveltejs/kit'
import { cms } from '$lib/norns/content/server/public'
import { settings } from '$lib/norns/settings/server/public'

// sessionHandle already refused anonymous and non-staff requests for every
// method; this is the friendly redirect plus the navigation tree (sections
// → items → submenus), filtered by the user's capabilities.
export load := page.load
	handler: async ({ user, url, container }) =>
		unless user
			throw redirect 303, `/login?next=${encodeURIComponent(url.pathname)}`
		site := await settings(container).get 'site'
		{
			user: { id: user.id, email: user.email, name: user.name, role: user.role, capabilities: user.capabilities, needs2fa: user.needs2fa }
			nav: cms(container).adminNav(user)
			siteTitle: site.title
		}
