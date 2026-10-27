import { page } from '@human-synthesis/norns/server'
import { error } from '@sveltejs/kit'
import { cms } from '$lib/norns/content/server/public'
import { articles } from '$lib/norns/articles/server/public'
import { seo } from '$lib/norns/seo/server/public'
import { consent } from '$lib/norns/consent/server/public'
import { settings } from '$lib/norns/settings/server/public'
import { i18n, tFor } from '$lib/norns/i18n/server/public'
import { auth, can, isStaff } from '$lib/norns/auth/server/public'

// The whole public site. localeHandle has resolved the localisation and
// language (locals.locale) and the path inside its base (`rest`); this
// matches `rest` against the language's URL words:
//
//   /                                  home (the chosen page's translation, else latest articles)
//   /<articles>                        article list          (/artikel in German, …)
//   /<articles>/<category>/<slug>      category archive
//   /<articles>/<tag>/<slug>           tag archive
//   /<authors>/<slug>                  author page
//   /<type section>/<slug>, /<slug>    entries (pages may be nested)
//   /cookies                           cookie declaration (until a page takes the URL)
//
// Every view returns `switcher`: where this same page lives in each
// language of the localisation, and in the other localisations.

// Links for the language and region switchers. `pathIn(code)` returns the
// equivalent path in a language, or null when the page has no version there.
switcherFor := (c: any, loc: any, pathIn: (code: string) => string | null) =>
	markets := i18n(c).markets()
	current := markets.find((m: any) => m.code === loc.market.code)
	languages := (current?.languages ?? []).map (l: any) =>
		info := i18n(c).language l.code
		{ base } := i18n(c).link l.code, current.code
		p := pathIn l.code
		{ code: l.code, name: info.native || info.name || l.code, href: base + (p ?? '/'), current: l.code === loc.language, exists: p !== null }
	regions := markets.filter((m: any) => m.code !== loc.market.code).map (m: any) =>
		code := m.languages.some((l: any) => l.code === loc.language) ? loc.language : m.defaultLanguage
		{ origin, base } := i18n(c).link code, m.code
		p := pathIn code
		{ code: m.code, name: m.name, href: origin + base + (p ?? '/') }
	{ languages, regions }

export load := page.load
	handler: async ({ container, user, event, url }) =>
		c := container
		loc := event.locals.locale
		lang := loc.language
		rest := loc.rest ?? '/'
		segs := rest.split('/').filter(Boolean)
		viewer := { preview: isStaff(user), member: can(user, 'content.members') }
		pageNo := Math.max 1, Number(url.searchParams.get('page')) || 1
		articleType := cms(c).getType 'article'
		section := (code: string) => articleType ? cms(c).sectionPath(articleType, code) : '/articles'
		word := (code: string, key: string) => i18n(c).word code, key
		t := await tFor c, lang

		// Home.
		if segs.length === 0
			home := await seo(c).homeEntry()
			if home
				res := await cms(c).resolve `/${home.slug}`, viewer
				if res && res.kind === 'entry'
					site := await settings(c).localized 'site'
					head := await seo(c).forEntry res
					title := res.entry.data.seoTitle ? head.title : (site.tagline ? `${site.title} · ${site.tagline}` : site.title)
					return { kind: 'page', res, seo: { ...head, title, canonical: head.canonical.replace(new RegExp(`/${home.slug}$`), '/'), alternates: seo(c).languageAlternates('/') }, switcher: switcherFor(c, loc, => '/') }
			list := await articles(c).list { perPage: 9, member: viewer.member }
			site := await settings(c).localized 'site'
			return { kind: 'latest', articles: list.entries, more: list.pages > 1 ? loc.base + section(lang) : null, seo: { ...(await seo(c).forPage({ path: `${loc.base}/`, description: site.tagline })), alternates: seo(c).languageAlternates('/') }, switcher: switcherFor(c, loc, => '/') }

		// Articles: list and archives.
		if articleType && `/${segs[0]}` === section(lang)
			if segs.length === 1
				featured := pageNo === 1 ? (await articles(c).list({ perPage: 3, featured: true, member: viewer.member })).entries : []
				list := await articles(c).list { page: pageNo, perPage: 12, member: viewer.member }
				cats := (await articles(c).terms('category')).filter((x) => x.count > 0).map((x) => articles(c).localized(x, lang))
				path := loc.base + section(lang)
				label := t('type.article.plural')
				return {
					kind: 'articles', featured, entries: list.entries.filter((e) => !featured.some((f) => f.id === e.id)), page: list.page, pages: list.pages, categories: cats, path
					seo: await seo(c).forPage({ title: pageNo > 1 ? `${label} — ${t('pager.page', { n: pageNo })}` : label, path: pageNo > 1 ? `${path}?page=${pageNo}` : path, crumbs: [{ name: label, url: path }], alternates: seo(c).languageAlternates(section(lang)) })
					switcher: switcherFor(c, loc, (code) => section(code))
				}
			if segs.length === 3 && (segs[1] === word(lang, 'category') || segs[1] === word(lang, 'tag'))
				tax := segs[1] === word(lang, 'category') ? 'category' : 'tag'
				term := await articles(c).termByLocalSlug tax, segs[2], lang
				unless term
					throw error 404, t('notFound.title')
				local := articles(c).localized term, lang
				list := await articles(c).list { page: pageNo, perPage: 12, termId: term.id, member: viewer.member }
				return {
					kind: 'archive', label: t(tax === 'category' ? 'archive.category' : 'archive.tag'), term: local, list, path: local.url
					feed: tax === 'category' ? `${local.url}/feed.xml` : null
					seo: await seo(c).forPage({ title: local.name, description: local.description || undefined, path: pageNo > 1 ? `${local.url}?page=${pageNo}` : local.url, noindex: list.total === 0, crumbs: [{ name: t('type.article.plural'), url: loc.base + section(lang) }, { name: local.name, url: local.url }] })
					switcher: switcherFor(c, loc, (code) => articles(c).termUrl(term, code).replace(i18n(c).link(code).base, ''))
				}

		// Author pages.
		if segs.length === 2 && segs[0] === word(lang, 'authors')
			author := await auth(c).authorBySlug segs[1]
			unless author
				throw error 404, t('notFound.title')
			list := await articles(c).byAuthor author.id, { page: pageNo, member: viewer.member }
			avatar := author.avatar ? (await cms(c).lookupMedia([author.avatar]))[author.avatar] : null
			path := `${loc.base}/${segs[0]}/${author.slug}`
			return {
				kind: 'author', author, avatar, list, path
				seo: await seo(c).forPage({ title: author.name, description: author.bio || t('archive.byAuthor', { name: author.name }), path, noindex: list.total === 0, crumbs: [{ name: author.name, url: path }] })
				switcher: switcherFor(c, loc, (code) => `/${word(code, 'authors')}/${author.slug}`)
			}

		// Entries.
		res := await cms(c).resolve rest, viewer
		if res && res.kind === 'list'
			path := loc.base + rest
			return { kind: 'list', ct: res.ct, entries: res.entries, seo: await seo(c).forPage({ title: res.ct.labelPlural, path }), switcher: switcherFor(c, loc, (code) => cms(c).sectionPath(res.ct, code)) }
		if res && res.kind === 'entry'
			extra := { authors: [], category: null, tags: [], sources: [], related: [], avatars: {} } as Record<string, any>
			if res.ct.id === 'article'
				[decorated] := await articles(c).decorate [res.entry]
				extra.authors = decorated.authors
				extra.category = decorated.category
				extra.tags = decorated.tagTerms
				extra.sources = res.locked ? [] : articles(c).sources(res.entry)
				extra.related = await articles(c).related res.entry
				extra.avatars = await cms(c).lookupMedia extra.authors.map((p) => p.avatar).filter(Boolean)
			byLocale := Object.fromEntries res.alternates.map((a) => [a.locale, a.path])
			fallbackName := res.fallbackFrom ? (i18n(c).language(res.fallbackFrom).native || res.fallbackFrom) : null
			return { kind: 'entry', res, extra, fallbackName, seo: await seo(c).forEntry(res, extra), switcher: switcherFor(c, loc, (code) => byLocale[code] ?? null) }

		if rest === '/cookies'
			return { kind: 'cookies', declaration: await consent(c).declaration(), seo: await seo(c).forPage({ title: t('cookies.title'), path: `${loc.base}/cookies` }), switcher: switcherFor(c, loc, => '/cookies') }

		throw error 404, t('notFound.title')
