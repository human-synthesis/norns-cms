import { page } from '@human-synthesis/norns/server'
import { cms } from '$lib/norns/content/server/public'
import { settings } from '$lib/norns/settings/server/public'
import { seo } from '$lib/norns/seo/server/public'
import { consent } from '$lib/norns/consent/server/public'
import { i18n } from '$lib/norns/i18n/server/public'
import { isStaff } from '$lib/norns/auth/server/public'

// Site chrome for every public page, in the request's language
// (locals.locale from localeHandle): localized settings, navigation, the
// interface texts, language / region links, head tags and the cookie
// banner. Pages override `switcher` with links to their own translations.
export load := page.load
	handler: async ({ container, user, event, url }) =>
		c := container
		// The locale comes from the URL (localeHandle): reading the pathname
		// makes SvelteKit rerun this load on client-side navigation, so the
		// texts follow when the visitor switches language.
		url.pathname
		loc := event.locals.locale
		site := await settings(c).localized 'site'
		state := event.locals.consent ?? await consent(c).state(event)
		strings := await i18n(c).strings loc.language
		markets := i18n(c).markets()
		current := markets.find((m: any) => m.code === loc.market.code) ?? markets[0]
		homes := {
			languages: (current?.languages ?? []).map (l: any) =>
				info := i18n(c).language l.code
				{ code: l.code, name: info.native || info.name || l.code, href: (i18n(c).link(l.code, current.code).base || '') + '/', current: l.code === loc.language, exists: true }
			regions: markets.filter((m: any) => m.code !== current?.code).map (m: any) =>
				code := m.languages.some((l: any) => l.code === loc.language) ? loc.language : m.defaultLanguage
				l := i18n(c).link code, m.code
				{ code: m.code, name: m.name, href: `${l.origin}${l.base}/` }
		}
		{
			site: { title: site.title, tagline: site.tagline, footerText: site.footerText, registration: site.registration }
			nav: await cms(c).nav()
			user: user ? { name: user.name || user.email, staff: isStaff(user) } : null
			seoSite: await seo(c).siteHead()
			consent: { showBanner: state.showBanner, choice: state.choice, consentId: state.consentId, enabled: state.enabled }
			banner: state.enabled ? await consent(c).banner(state) : null
			i18n: { locale: loc.language, dir: loc.dir, base: loc.base, market: loc.market.code, marketName: loc.market.name, strings }
			homes
		}
