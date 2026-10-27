import { describe, test, expect } from 'bun:test';
import { withScope } from '@human-synthesis/norns/server';
import { bootApp } from './helpers.js';
import { resolveLocale, basePath, negotiate, hreflangs } from '../src/lib/norns/i18n/shared/urls.js';
import { i18n } from '../src/lib/norns/i18n/server/public.c';
import { cms } from '../src/lib/norns/content/server/public.c';
import { settings } from '../src/lib/norns/settings/server/public.c';
import { seo } from '../src/lib/norns/seo/server/public.c';
import { articles } from '../src/lib/norns/articles/server/public.c';
import { auth } from '../src/lib/norns/auth/server/public.c';

process.env.CMS_PUBLIC_URL = 'https://site.com';

const market = (o) => ({ id: 1, code: 'm', name: 'M', mode: 'path', prefix: '', host: '', languages: [{ code: 'en', segment: 'en' }], defaultLanguage: 'en', prefixDefault: false, countries: [], contentFallback: false, isPrimary: false, enabled: true, ...o });
const cfg = {
	primaryHost: 'site.com',
	protocol: 'https:',
	languages: {},
	markets: [
		market({ code: 'global', isPrimary: true, languages: [{ code: 'en', segment: 'en' }, { code: 'de', segment: 'de' }], prefixDefault: true }),
		market({ code: 'ch', prefix: 'ch', languages: [{ code: 'de-CH', segment: 'de' }, { code: 'fr-CH', segment: 'fr' }], defaultLanguage: 'de-CH', prefixDefault: true, countries: ['CH'] }),
		market({ code: 'de', mode: 'subdomain', host: 'de.site.com', languages: [{ code: 'de', segment: 'de' }], defaultLanguage: 'de', countries: ['DE'] }),
		market({ code: 'ro', mode: 'domain', host: 'site.ro', languages: [{ code: 'ro', segment: 'ro' }, { code: 'en', segment: 'en' }], defaultLanguage: 'ro', countries: ['RO'] })
	]
};

describe('URLs', () => {
	test('sub-folders, sub-domains and domains resolve to a localisation and language', () => {
		const r = (host, path, al) => {
			const x = resolveLocale(cfg, host, path, al);
			return x.redirect ? `→${x.status} ${x.redirect}` : `${x.market.code}/${x.language} ${x.base}|${x.rest}`;
		};
		expect(r('site.com', '/en/articles/x')).toBe('global/en /en|/articles/x');
		expect(r('site.com', '/de/')).toBe('global/de /de|/');
		expect(r('site.com', '/ch/fr/kontakt')).toBe('ch/fr-CH /ch/fr|/kontakt');
		expect(r('de.site.com', '/artikel/x')).toBe('de/de |/artikel/x');
		expect(r('site.ro', '/despre')).toBe('ro/ro |/despre');
		expect(r('site.ro', '/en/about')).toBe('ro/en /en|/about');
		expect(r('workers.example.dev', '/en/')).toBe('global/en /en|/');
	});

	test('when every language is prefixed: / negotiates (302), other paths go to the default (301)', () => {
		expect(resolveLocale(cfg, 'site.com', '/', 'de-DE,de;q=0.9,en;q=0.5')).toMatchObject({ redirect: '/de/', status: 302 });
		expect(resolveLocale(cfg, 'site.com', '/articles/x', 'de')).toMatchObject({ redirect: '/en/articles/x', status: 301 });
		expect(resolveLocale(cfg, 'site.com', '/ch', 'fr-CH')).toMatchObject({ redirect: '/ch/fr/', status: 302 });
	});

	test('bases, negotiation and hreflang regions', () => {
		expect(basePath(cfg.markets[1], 'fr-CH')).toBe('/ch/fr');
		expect(basePath(cfg.markets[3], 'ro')).toBe('');
		expect(negotiate('fr-FR;q=0.8, it;q=0.9', ['de-CH', 'fr-CH', 'it'], 'de-CH')).toBe('it');
		expect(negotiate('fr-BE', ['de-CH', 'fr-CH'], 'de-CH')).toBe('fr-CH');
		expect(negotiate('', ['en'], 'en')).toBe('en');
		expect(hreflangs(cfg.markets[2], 'de')).toEqual(['de-DE']);
		expect(hreflangs(cfg.markets[1], 'de-CH')).toEqual(['de-CH']);
		expect(hreflangs(cfg.markets[0], 'en')).toEqual(['en']);
	});
});

// A booted app with English + German on sub-folders and German on a sub-domain.
async function setup() {
	const c = await bootApp();
	await i18n(c).saveLanguage({ code: 'de', paths: { article: 'artikel', authors: 'autoren', category: 'kategorie', tag: 'schlagwort' } });
	await i18n(c).saveLanguage({ code: 'de-CH', fallback: 'de' });
	const [primary] = await i18n(c).allMarkets();
	expect((await i18n(c).saveMarket(primary.id, { ...primary, languages: [{ code: 'en' }, { code: 'de' }], prefixDefault: true })).ok).toBe(true);
	expect((await i18n(c).saveMarket(null, { code: 'ch', name: 'Switzerland', mode: 'path', prefix: 'ch', languages: [{ code: 'de-CH', segment: 'de' }], defaultLanguage: 'de-CH', countries: 'CH', contentFallback: true })).ok).toBe(true);
	expect((await i18n(c).saveMarket(null, { code: 'de', name: 'Deutschland', mode: 'subdomain', host: 'de.site.com', languages: [{ code: 'de' }], defaultLanguage: 'de', countries: 'DE' })).ok).toBe(true);
	await i18n(c).config();
	const ownerId = await auth(c).createUser('o@example.com', 'Olga', 'correct horse battery', 'owner');
	const owner = { ...(await auth(c).getUser(ownerId)), capabilities: ['*'] };
	return { c, owner };
}

// Run fn as a request to `url`, resolved the way localeHandle does it.
async function at(c, url, fn) {
	const u = new URL(url);
	const event = { url: u, locals: { container: c }, request: new Request(u), cookies: { get: () => undefined }, platform: {} };
	event.locals.i18nConfig = await i18n(c).config();
	return withScope({ container: c, event }, async () => {
		const svc = c.resolve('i18n.service');
		const r = svc.resolve(u.host, u.pathname, '');
		event.locals.locale = svc.describe(r);
		return fn(event.locals.locale);
	});
}

const publish = async (c, type, raw, user, extra) => {
	const r = await cms(c).save(type, null, raw, user, extra);
	if (!r.ok) throw new Error(JSON.stringify(r.issues));
	await cms(c).setStatus(type, r.id, 'published');
	return cms(c).get(type, r.id);
};

describe('localisation config', () => {
	test('two localisations cannot claim the same URLs', async () => {
		const { c } = await setup();
		const save = (o) => i18n(c).saveMarket(null, { name: 'X', languages: [{ code: 'en' }], ...o });
		expect((await save({ code: 'x', mode: 'path' })).error).toContain('Only one sub-folder');
		expect((await save({ code: 'y', mode: 'path', prefix: 'ch' })).error).toContain('prefix');
		expect((await save({ code: 'z', mode: 'path', prefix: 'de' })).error).toContain('language segment');
		expect((await save({ code: 'w', mode: 'subdomain', host: 'de.site.com' })).error).toContain('host');
		expect((await save({ code: 'v', mode: 'domain', host: 'site.fr', languages: [{ code: 'fr' }] })).error).toContain('Languages first');
		expect((await save({ code: 'ch', mode: 'domain', host: 'site.at' })).error).toContain('code');
	});

	test('fallback chains and interface texts', async () => {
		const { c } = await setup();
		expect(i18n(c).chain('de-CH')).toEqual(['de-CH', 'de']);
		expect((await i18n(c).strings('de-CH'))['home.latest']).toBe('Neueste Artikel');
		await i18n(c).setStrings('de-CH', { 'home.latest': 'Neuschti Artikel' });
		expect((await i18n(c).strings('de-CH'))['home.latest']).toBe('Neuschti Artikel');
		expect((await i18n(c).strings('de'))['home.latest']).toBe('Neueste Artikel');
		const row = (await i18n(c).stringTable('de-CH')).find((r) => r.key === 'home.latest');
		expect(row.override).toBe('Neuschti Artikel');
	});
});

describe('translated content', () => {
	test('a translation is a linked draft with the same slug, in its own language', async () => {
		const { c, owner } = await setup();
		const en = await publish(c, 'article', { title: 'Hello', body: '## Hi', tags: 'news' }, owner);
		const tr = await cms(c).translate('article', en.id, 'de', owner, { ...en.data, title: 'Hallo', body: '## Hallo' });
		expect(tr.ok).toBe(true);
		expect(tr.entry).toMatchObject({ locale: 'de', slug: 'hello', status: 'draft', translation_group: en.translation_group, translation_of: en.id });
		expect((await cms(c).translate('article', en.id, 'de', owner)).existing).toBe(tr.id);
		expect((await cms(c).translate('article', en.id, 'fr', owner)).ok).toBe(false);
		await cms(c).setStatus('article', tr.id, 'published');
		const states = await cms(c).translations(en);
		expect(states.map((s) => [s.locale, s.isSource, s.outdated])).toEqual([['en', true, false], ['de', false, false]]);
		await new Promise((r) => setTimeout(r, 5));
		await cms(c).save('article', en.id, { ...cms(c).rawFrom(cms(c).getType('article'), en.data), title: 'Hello again', slug: en.slug }, owner);
		expect((await cms(c).translations(en)).find((s) => s.locale === 'de').outdated).toBe(true);
		await cms(c).markCurrent(await cms(c).get('article', tr.id));
		expect((await cms(c).translations(en)).find((s) => s.locale === 'de').outdated).toBe(false);
	});

	test('each localisation shows its language at its own URLs, with hreflang alternates', async () => {
		const { c, owner } = await setup();
		const en = await publish(c, 'article', { title: 'Hello', body: 'EN body' }, owner);
		const tr = await cms(c).translate('article', en.id, 'de', owner, { ...en.data, title: 'Hallo', body: 'DE body' });
		await cms(c).setStatus('article', tr.id, 'published');
		await at(c, 'https://site.com/en/articles/hello', async (loc) => {
			const res = await cms(c).resolve(loc.rest, {});
			expect(res.entry.title).toBe('Hello');
			const head = await seo(c).forEntry(res);
			expect(head.canonical).toBe('https://site.com/en/articles/hello');
			const alt = Object.fromEntries(head.alternates.map((a) => [a.hreflang, a.href]));
			expect(alt).toEqual({ en: 'https://site.com/en/articles/hello', de: 'https://site.com/de/artikel/hello', 'de-DE': 'https://de.site.com/artikel/hello', 'x-default': 'https://site.com/en/articles/hello' });
		});
		await at(c, 'https://site.com/de/artikel/hello', async (loc) => {
			expect((await cms(c).resolve(loc.rest, {})).entry.title).toBe('Hallo');
			expect(await cms(c).resolve('/articles/hello', {})).toBeNull();
		});
		await at(c, 'https://de.site.com/artikel/hello', async (loc) => {
			const res = await cms(c).resolve(loc.rest, {});
			expect(res.entry.title).toBe('Hallo');
			expect(cms(c).urlFor(cms(c).getType('article'), res.entry)).toBe('/artikel/hello');
		});
	});

	test('content fallback and availability per localisation', async () => {
		const { c, owner } = await setup();
		const de = await publish(c, 'page', { title: 'Impressum', layout: 'markdown', body: 'Text' }, owner, { locale: 'de' });
		await at(c, 'https://site.com/ch/de/impressum', async (loc) => {
			const res = await cms(c).resolve(loc.rest, {});
			expect(res.entry.locale).toBe('de');
			expect(res.fallbackFrom).toBe('de');
			const head = await seo(c).forEntry(res);
			expect(head.canonical).toBe('https://site.com/de/impressum');
		});
		await cms(c).save('page', de.id, { title: 'Impressum', layout: 'markdown', body: 'Text', slug: 'impressum', markets: ['de'] }, owner);
		await at(c, 'https://site.com/de/impressum', async (loc) => expect(await cms(c).resolve(loc.rest, {})).toBeNull());
		await at(c, 'https://de.site.com/impressum', async (loc) => expect((await cms(c).resolve(loc.rest, {})).entry.title).toBe('Impressum'));
	});

	test('navigation, lists and archives follow the language', async () => {
		const { c, owner } = await setup();
		await publish(c, 'page', { title: 'About', layout: 'markdown', showInNav: 'on' }, owner, { locale: 'en' });
		await publish(c, 'page', { title: 'Über uns', layout: 'markdown', showInNav: 'on' }, owner, { locale: 'de' });
		const cat = await articles(c).saveTerm('category', null, { name: 'Guides', translations: { de: { name: 'Anleitungen', slug: 'anleitungen' } } });
		await publish(c, 'article', { title: 'Wie', category: String(cat.id) }, owner, { locale: 'de' });
		await at(c, 'https://site.com/de/', async () => {
			expect(await cms(c).nav()).toEqual([{ label: 'Über uns', href: '/de/uber-uns' }, { label: 'Articles', href: '/de/artikel', typeId: 'article' }]);
			expect((await articles(c).list()).entries.map((e) => e.title)).toEqual(['Wie']);
			const [d] = (await articles(c).list()).entries;
			expect(d.category).toMatchObject({ name: 'Anleitungen', url: '/de/artikel/kategorie/anleitungen' });
			expect((await articles(c).termByLocalSlug('category', 'anleitungen', 'de')).id).toBe(cat.id);
		});
		await at(c, 'https://site.com/en/', async () => expect((await articles(c).list()).entries).toHaveLength(0));
	});
});

describe('translated texts around content', () => {
	test('translatable settings per language, kept when base values change', async () => {
		const { c } = await setup();
		await settings(c).save('site', { title: 'Site', tagline: 'Hello', registration: 'off' }, null);
		await settings(c).saveTranslation('site', 'de', { title: '', tagline: 'Hallo', registration: 'open' }, null);
		await settings(c).save('site', { title: 'Site 2', tagline: 'Hello 2', registration: 'off' }, null);
		expect((await settings(c).localized('site', 'de')).tagline).toBe('Hallo');
		expect((await settings(c).localized('site', 'de')).title).toBe('Site 2');
		expect((await settings(c).localized('site', 'de')).registration).toBe('off');
		expect((await settings(c).localized('site', 'de-CH')).tagline).toBe('Hallo');
		expect((await settings(c).get('site')).tagline).toBe('Hello 2');
	});

	test('sitemaps, feeds and llms.txt per locale and host', async () => {
		const { c, owner } = await setup();
		const en = await publish(c, 'article', { title: 'Hello' }, owner);
		const tr = await cms(c).translate('article', en.id, 'de', owner);
		await cms(c).setStatus('article', tr.id, 'published');
		await at(c, 'https://site.com/sitemap.xml', async () => {
			const xml = await seo(c).sitemapIndex();
			expect(xml).toContain('https://site.com/en/sitemap-article.xml');
			expect(xml).toContain('https://site.com/ch/sitemap-article.xml');
			expect(xml).not.toContain('de.site.com');
		});
		await at(c, 'https://de.site.com/sitemap.xml', async () => {
			const xml = await seo(c).sitemapIndex();
			expect(xml).toContain('https://de.site.com/sitemap-article.xml');
			expect(xml).not.toContain('https://site.com/');
		});
		await at(c, 'https://site.com/de/sitemap-article.xml', async () => {
			const xml = await seo(c).sitemapFor('article');
			expect(xml).toContain('<loc>https://site.com/de/artikel/hello</loc>');
			expect(xml).toContain('hreflang="en" href="https://site.com/en/articles/hello"');
		});
		await at(c, 'https://site.com/de/llms.txt', async () => {
			const txt = await seo(c).llms();
			expect(txt).toContain('https://site.com/de/artikel/hello.md');
			expect(txt).toContain('https://site.com/en/llms.txt');
		});
		await at(c, 'https://site.com/de/feed.xml', async () => expect(await seo(c).feed()).toContain('<language>de</language>'));
	});

	test('a member registering on the German site gets German emails', async () => {
		const { c } = await setup();
		await settings(c).patch('site', { registration: 'open' }, null);
		await at(c, 'https://site.com/de/', async () => {
			await auth(c).register({ email: 'm@example.com', name: 'M', password: 'blue harbour 77' }, '1.1.1.1');
		});
		const { mail } = await import('../src/lib/norns/mail/server/public.c');
		const m = await mail(c).lastTo('m@example.com');
		expect(m.subject).toContain('Bestätigen Sie Ihre E-Mail-Adresse');
		expect((await auth(c).userByEmail('m@example.com')).locale).toBe('de');
	});
});

describe('forms', () => {
	test('switches posted as hidden "false" + "on" keep their value; the primary stays enabled', async () => {
		const { c } = await setup();
		const [primary] = await i18n(c).allMarkets();
		const r = await i18n(c).saveMarket(primary.id, { ...primary, languages: [{ code: 'en' }], prefixDefault: ['false', 'on'], isPrimary: ['false', 'on'], enabled: ['false'] });
		expect(r.ok).toBe(true);
		const saved = await i18n(c).market(primary.id);
		expect(saved.prefixDefault).toBe(true);
		expect(saved.enabled).toBe(true);
	});
});
