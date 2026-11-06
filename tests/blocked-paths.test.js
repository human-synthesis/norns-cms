import { describe, test, expect } from 'bun:test';
import { bootApp } from './helpers.js';
import { blocked, blockHandle, countryFilter } from '../src/lib/norns/protection/server/public.c';
import { allowed, cleanFilter, countryOptions } from '../src/lib/norns/protection/shared/countries.js';
import { seo } from '../src/lib/norns/seo/server/public.c';
import { DEFAULT_PATTERNS, PROTECTED, matches, patternProblem, looksLikeProbe, suggestPattern, normalizePattern } from '../src/lib/norns/protection/shared/paths.js';

// A request through the handle; `next` records whether the rest of the
// chain ran.
async function request(c, path, waits = [], country = '') {
	let reached = false;
	const url = new URL(path, 'https://site.example');
	const event = { url, request: new Request(url), locals: { container: c }, platform: { cf: { country }, context: { waitUntil: (p) => waits.push(p) } } };
	const res = await blockHandle()({ event, resolve: async () => ((reached = true), new Response('page')) });
	return { status: res.status, reached, body: await res.text() };
}

describe('patterns', () => {
	test('* matches anything, case-insensitive, whole path', () => {
		expect(matches('*.php', '/a/b/Login.PHP')).toBe(true);
		expect(matches('*.php', '/notes.php.html')).toBe(false);
		expect(matches('/wp-admin*', '/wp-admin')).toBe(true);
		expect(matches('/wp-admin*', '/wp-admin/install.php')).toBe(true);
		expect(matches('/wp-admin*', '/blog/wp-admin')).toBe(false);
		expect(matches('/.env*', '/.env.production')).toBe(true);
		expect(normalizePattern('https://site.example/WP-Login.php?x=1')).toBe('/wp-login.php');
		expect(normalizePattern('wp-login.php')).toBe('/wp-login.php');
	});

	test('the starting list never blocks what the site serves; such patterns are refused', () => {
		for (const [p] of DEFAULT_PATTERNS) expect(patternProblem(p), p).toBe('');
		for (const [p] of DEFAULT_PATTERNS) for (const x of PROTECTED) expect(matches(p, x), `${p} vs ${x}`).toBe(false);
		expect(patternProblem('/admin*')).toContain('/admin');
		expect(patternProblem('*.txt')).toContain('/robots.txt');
		expect(patternProblem('*.xml')).toContain('/sitemap.xml');
		expect(patternProblem('*')).toContain('Enter a path');
		expect(patternProblem('/')).toContain('Enter a path');
	});

	test('probe detection and suggested patterns', () => {
		for (const p of ['/wp-login.php', '/.git/config', '/.env', '/cgi-bin/luci', '/phpmyadmin/index', '/vendor/phpunit/x', '/a/../../etc/passwd', '/backup.sql.gz'])
			expect(looksLikeProbe(p), p).toBe(true);
		for (const p of ['/about', '/articles/my-post', '/de/artikel/hallo', '/pricing'])
			expect(looksLikeProbe(p), p).toBe(false);
		expect(suggestPattern('/blog/xmlrpc.php')).toBe('*.php');
		expect(suggestPattern('/.git/HEAD')).toBe('*/.git*');
		expect(suggestPattern('/en/.npmrc')).toBe('*/.npmrc*');
		expect(matches('*/.npmrc*', '/.npmrc')).toBe(true);
		expect(suggestPattern('/wp-content/plugins/x/readme.txt')).toBe('/wp-content*');
		expect(suggestPattern('/solr/admin/info')).toBe('/solr*');
		expect(suggestPattern('/old-page')).toBe('/old-page');
	});
});

describe('the handle', () => {
	test('blocked paths get an empty 403 and stop the chain; others pass; hits are counted', async () => {
		const c = await bootApp();
		const waits = [];
		for (const p of ['/wp-login.php', '/.env', '/WP-ADMIN/setup-config.php', '/%2egit/config', '/vendor/phpunit/src/eval-stdin.php'])
			expect(await request(c, p, waits), p).toEqual({ status: 403, reached: false, body: '' });
		for (const p of ['/', '/articles/hello', '/admin', '/robots.txt', '/sitemap.xml', '/llms.txt'])
			expect((await request(c, p)).reached, p).toBe(true);
		await blocked(c).flushHits();
		const rows = await blocked(c).list();
		expect(rows.find((r) => r.pattern === '*.php').hits).toBe(3);
		const envRule = (await blocked(c).check('/.env')).pattern; // the first of the patterns covering it
		expect(rows.find((r) => r.pattern === envRule).last_hit_at).toBeGreaterThan(0);
	});

	test('adding, pausing and removing take effect at once', async () => {
		const c = await bootApp();
		expect((await request(c, '/solr/admin')).reached).toBe(true);
		const r = await blocked(c).add({ pattern: 'solr*', note: 'Solr' });
		expect(r).toMatchObject({ ok: true, pattern: '/solr*', existed: false });
		expect((await request(c, '/solr/admin')).status).toBe(403);
		await blocked(c).update(r.row.id, { enabled: false });
		expect((await request(c, '/solr/admin')).reached).toBe(true);
		await blocked(c).update(r.row.id, { enabled: true });
		expect((await request(c, '/solr/admin')).status).toBe(403);
		await blocked(c).remove(r.row.id);
		expect((await request(c, '/solr/admin')).reached).toBe(true);
		expect((await blocked(c).add({ pattern: '/media*' })).error).toContain('/media');
		expect((await blocked(c).add({ pattern: '*.php' })).existed).toBe(true);
	});

	test('the starting list can be added again after removals', async () => {
		const c = await bootApp();
		const all = await blocked(c).list();
		expect(all).toHaveLength(DEFAULT_PATTERNS.length);
		await blocked(c).remove(all[0].id);
		await blocked(c).remove(all[1].id);
		expect(await blocked(c).restoreDefaults()).toBe(2);
		expect(await blocked(c).restoreDefaults()).toBe(0);
	});
});

describe('from the 404 log', () => {
	test('probe-like 404s are grouped under a suggested pattern; moving one blocks it and clears the log', async () => {
		const c = await bootApp();
		await blocked(c).remove((await blocked(c).list()).find((r) => r.pattern === '/.git*').id);
		for (const [p, n] of [['/.git/config', 3], ['/.git/HEAD', 2], ['/old-pricing', 5], ['/solr/admin/cores', 1]])
			for (let i = 0; i < n; i++) await c.resolve('seo.service').log404(p, '');
		const s = await seo(c).probeSuggestions();
		expect(s.map((g) => [g.pattern, g.hits])).toEqual([['*/.git*', 5], ['/solr*', 1]]);
		const r = await seo(c).blockFromLog('*/.git*');
		expect(r).toMatchObject({ ok: true, cleared: 2 });
		expect((await seo(c).notFound()).map((n) => n.path).sort()).toEqual(['/old-pricing', '/solr/admin/cores']);
		expect((await blocked(c).list()).find((x) => x.pattern === '*/.git*').source).toBe('404');
		expect((await request(c, '/.git/config')).status).toBe(403);
		expect((await seo(c).blockFromLog('/sitemap.xml')).ok).toBe(false);
	});
});

describe('country filter', () => {
	const from = (c, country, path = '/') => request(c, path, [], country);

	test('off by default; allow-list lets only those countries in, the team pages and cron stay open', async () => {
		const c = await bootApp();
		expect((await from(c, 'US')).reached).toBe(true);
		expect((await countryFilter(c).save({ mode: 'allow', countries: ['ro', 'DE', 'nope'] }, { country: 'RO' })).filter).toEqual({ mode: 'allow', countries: ['DE', 'RO'], unknown: 'allow', admin: false });
		expect((await from(c, 'RO')).reached).toBe(true);
		expect(await from(c, 'US')).toEqual({ status: 403, reached: false, body: '' });
		expect(await from(c, 'US', '/articles/x')).toMatchObject({ status: 403 });
		expect((await from(c, 'US', '/admin')).reached).toBe(true);
		expect((await from(c, 'US', '/login')).reached).toBe(true);
		expect((await from(c, 'US', '/api/v1/content')).reached).toBe(true);
		expect((await from(c, '', '/api/cron')).reached).toBe(true);
		expect((await from(c, '')).reached).toBe(true); // unknown: let in
		await countryFilter(c).save({ mode: 'allow', countries: ['RO'], unknown: 'block' }, { country: 'RO' });
		expect((await from(c, '')).status).toBe(403);
		expect((await from(c, 'XX')).status).toBe(403);
		expect((await from(c, '', '/api/cron')).reached).toBe(true);
	});

	test('block-list refuses only those; Tor can be listed; blocked paths still win', async () => {
		const c = await bootApp();
		await countryFilter(c).save({ mode: 'block', countries: ['CN', 'T1'] }, {});
		expect((await from(c, 'CN')).status).toBe(403);
		expect((await from(c, 'T1')).status).toBe(403);
		expect((await from(c, 'US')).reached).toBe(true);
		expect((await from(c, 'US', '/wp-login.php')).status).toBe(403);
		expect(countryOptions().find((o) => o.value === 'T1').label).toBe('Tor network');
	});

	test('filtering the admin is refused when it would lock out the person saving', async () => {
		const c = await bootApp();
		const lock = await countryFilter(c).save({ mode: 'allow', countries: ['RO'], admin: true }, { country: 'US' });
		expect(lock.ok).toBe(false);
		expect(lock.error).toContain('United States');
		expect((await countryFilter(c).save({ mode: 'allow', countries: ['RO'], admin: true, unknown: 'block' }, { country: '' })).error).toContain('unknown country');
		expect((await countryFilter(c).save({ mode: 'allow', countries: ['RO'], admin: true, unknown: 'allow' }, { country: '' })).ok).toBe(true); // unknown is let in
		await countryFilter(c).save({ mode: 'off', countries: [] }, {});
		expect((await countryFilter(c).save({ mode: 'allow', countries: [] }, {})).error).toContain('at least one');
		expect((await countryFilter(c).save({ mode: 'allow', countries: ['RO'], admin: 'on' }, { country: 'RO' })).ok).toBe(true);
		expect((await from(c, 'US', '/admin')).status).toBe(403);
		expect((await from(c, 'RO', '/admin')).reached).toBe(true);
	});

	test('refusals are counted per country', async () => {
		const c = await bootApp();
		await countryFilter(c).save({ mode: 'block', countries: ['CN', 'RU'] }, {});
		for (const cc of ['CN', 'CN', 'RU']) await from(c, cc);
		const stats = await countryFilter(c).stats();
		expect(stats.map((x) => [x.country, x.name, x.hits])).toEqual([['CN', 'China', 2], ['RU', 'Russia', 1]]);
		await countryFilter(c).clearStats();
		expect(await countryFilter(c).stats()).toEqual([]);
	});

	test('pure decision', () => {
		const f = cleanFilter({ mode: 'block', countries: 'cn, ru', admin: 'true' });
		expect(f).toEqual({ mode: 'block', countries: ['CN', 'RU'], unknown: 'allow', admin: true });
		expect(allowed(f, 'CN', '/admin')).toBe(false);
		expect(allowed(f, 'cn', '/')).toBe(false);
		expect(allowed({ ...f, mode: 'off' }, 'CN', '/')).toBe(true);
	});
});
