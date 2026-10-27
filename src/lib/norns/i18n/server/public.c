import type { Container } from '@human-synthesis/norns/server'
import type { I18nService } from './service'
import { translate } from '../shared/t.js'

export { DEFAULT_CONFIG } from './service'
export { translate }
export { basePath, baseFor, negotiate, hreflangs } from '../shared/urls.js'
import { collectStrings, applyStrings, translateStrings } from './translator.js'
import { envVar } from '../../settings/server/public'

// Machine translation is offered only when ANTHROPIC_API_KEY is set.
export aiAvailable := => !!envVar('ANTHROPIC_API_KEY')

languageLabel := (c: Container, code: string) =>
	l := svc(c).language code
	`${l.name || code} (${code})`

// Translate an entry's data from one language to another (strings only).
export aiTranslateData := async (c: Container, fields: any[], data: Record<string, any>, blockLookup: (id: string) => any, from: string, to: string) =>
	strings := collectStrings fields, data, blockLookup
	translated := await translateStrings strings, { from: languageLabel(c, from), to: languageLabel(c, to), apiKey: envVar('ANTHROPIC_API_KEY'), model: envVar('ANTHROPIC_MODEL'), context: 'A page or article of a website' }
	applyStrings data, translated

// Fill the interface texts a language has no translation for.
export aiTranslateTexts := async (c: Container, code: string) =>
	rows := await svc(c).stringTable code
	missing := Object.fromEntries rows.filter((r: any) => !r.override && !r.builtIn).map((r: any) => [r.key, r.english])
	return 0 unless Object.keys(missing).length
	translated := await translateStrings missing, { from: 'English (en)', to: languageLabel(c, code), apiKey: envVar('ANTHROPIC_API_KEY'), model: envVar('ANTHROPIC_MODEL'), context: 'Interface texts of a website (buttons, labels, cookie banner, account pages, emails). Keep them as short as the English.' }
	await svc(c).setStrings code, translated

svc := (c: Container) => c.resolve('i18n.service') as I18nService

export i18n := (c: Container) => {
	config: () => svc(c).config()
	markets: () => svc(c).markets()
	primary: () => svc(c).primary()
	language: (code: string) => svc(c).language(code)
	contentLanguages: () => svc(c).contentLanguages()
	defaultLanguage: () => svc(c).defaultLanguage()
	chain: (code: string) => svc(c).chain(code)
	link: (code: string, marketCode?: string) => svc(c).link(code, marketCode)
	placesOf: (code: string) => svc(c).placesOf(code)
	typeSegment: (code: string, typeId: string, fallback: string) => svc(c).typeSegment(code, typeId, fallback)
	word: (code: string, key: string) => svc(c).word(code, key)
	strings: (code: string) => svc(c).strings(code)
	stringTable: (code: string) => svc(c).stringTable(code)
	setStrings: (code: string, values: Record<string, string>) => svc(c).setStrings(code, values)
	known: () => svc(c).known()
	allLanguages: () => svc(c).allLanguages()
	saveLanguage: (input: any) => svc(c).saveLanguage(input)
	deleteLanguage: (code: string) => svc(c).deleteLanguage(code)
	allMarkets: () => svc(c).allMarkets()
	market: (id: number) => svc(c).market(id)
	saveMarket: (id: number | null, input: any) => svc(c).saveMarket(id, input)
	deleteMarket: (id: number) => svc(c).deleteMarket(id)
	map: () => svc(c).map()
}

// t() for server code that has the request's strings (emails, errors).
export tFor := async (c: Container, code: string) =>
	strings := await svc(c).strings code
	(key: string, vars?: Record<string, any>) => translate(strings, key, vars)

SYSTEM := /^\/(admin|api|account|login|media|_app|cdn-cgi|reset|invite|verify|forms|favicon)(\/|$|\.)/
ROOT_FILES := new Set(['/robots.txt', '/sitemap.xml'])
LANG_COOKIE := 'cms_lang'

// Works out the localisation and language of every request:
//   public pages   from host + path (redirecting to a language when every
//                  language of the localisation is prefixed),
//   system pages   (/admin, /account, /api …) from the cms_lang cookie,
// into event.locals.locale, and sets <html lang dir>.
export localeHandle := =>
	async ({ event, resolve }: any) =>
		c := event.locals.container
		s := svc c
		event.locals.i18nConfig = await s.config()
		path := event.url.pathname
		host := event.url.host
		if SYSTEM.test(path) || ROOT_FILES.has(path)
			event.locals.locale = s.systemLocale host, event.cookies.get(LANG_COOKIE)
		else
			r := s.resolve host, path, event.request.headers.get('accept-language') ?? ''
			if r?.redirect
				return new Response(null, { status: r.status, headers: { location: r.redirect + event.url.search, vary: 'Accept-Language' } })
			event.locals.locale = r ? s.describe(r) : s.systemLocale(host, undefined)
			if event.cookies.get(LANG_COOKIE) !== event.locals.locale.language
				event.cookies.set LANG_COOKIE, event.locals.locale.language, { path: '/', httpOnly: false, sameSite: 'lax', secure: event.url.protocol === 'https:', maxAge: 60 * 60 * 24 * 365 }
		loc := event.locals.locale
		admin := path.startsWith('/admin') || path === '/login' || path.startsWith('/login/')
		lang := admin ? 'en' : loc.language
		dir := admin ? 'ltr' : loc.dir
		resolve event, {
			transformPageChunk: ({ html }: any) => html.replace('%cms.lang%', lang).replace('%cms.dir%', dir)
		}
