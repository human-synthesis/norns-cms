import { page } from '@human-synthesis/norns/server'
import { cms } from '$lib/norns/content/server/public'
import { settings } from '$lib/norns/settings/server/public'
import { seo } from '$lib/norns/seo/server/public'
import { consent } from '$lib/norns/consent/server/public'
import { isStaff } from '$lib/norns/auth/server/public'

// Site chrome for every public page: settings, navigation, who is signed
// in, site-wide head tags and the cookie banner. consentHandle has already
// worked out this visitor's consent state (locals.consent).
export load := page.load
	handler: async ({ container, user, event }) =>
		site := await settings(container).get 'site'
		state := event.locals.consent ?? await consent(container).state(event)
		{
			site: { title: site.title, tagline: site.tagline, footerText: site.footerText, registration: site.registration }
			nav: await cms(container).nav()
			user: user ? { name: user.name || user.email, staff: isStaff(user) } : null
			seoSite: await seo(container).siteHead()
			consent: { showBanner: state.showBanner, choice: state.choice, consentId: state.consentId, enabled: state.enabled }
			banner: state.enabled ? await consent(container).banner(state) : null
		}
