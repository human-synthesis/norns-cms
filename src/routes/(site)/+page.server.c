import { page } from '@human-synthesis/norns/server'
import { cms } from '$lib/norns/content/server/public'
import { settings } from '$lib/norns/settings/server/public'
import { articles } from '$lib/norns/articles/server/public'
import { seo } from '$lib/norns/seo/server/public'
import { can, isStaff } from '$lib/norns/auth/server/public'

// The home page: the page chosen under Settings → Site (usually a blocks
// page), or else the latest articles.
export load := page.load
	handler: async ({ container, user }) =>
		site := await settings(container).get 'site'
		viewer := { preview: isStaff(user), member: can(user, 'content.members') }
		if site.homePage
			home := await cms(container).byId Number(site.homePage)
			if home
				res := await cms(container).resolve `/${home.slug}`, viewer
				if res && res.kind === 'entry'
					head := await seo(container).forEntry res
					title := res.entry.data.seoTitle ? head.title : (site.tagline ? `${site.title} · ${site.tagline}` : site.title)
					return { kind: 'page', res, seo: { ...head, title, canonical: head.canonical.replace(`/${home.slug}`, '/') } }
		list := await articles(container).list { perPage: 9, member: viewer.member }
		{ kind: 'latest', articles: list.entries, more: list.pages > 1, seo: await seo(container).forPage({ path: '/', description: site.tagline }) }
