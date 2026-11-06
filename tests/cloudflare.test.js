import { describe, test, expect, beforeEach, afterEach } from 'bun:test';
import { bootApp } from './helpers.js';
import { cloudflare, blocked, countryFilter } from '../src/lib/norns/protection/server/public.c';
import { pathRules, countryRule, zoneCandidates, MAX_EXPRESSION } from '../src/lib/norns/protection/shared/firewall.js';

// A small stand-in for the Cloudflare API: one zone, the custom-rules
// entrypoint (missing at first), and a rule someone else made.
function fakeCloudflare({ preexisting = true, token = 'good-token' } = {}) {
	const state = { ruleset: null, calls: [], nextId: 1 };
	if (preexisting) state.pending = [{ id: 'theirs', ref: 'my_rule', description: 'Mine', expression: 'ip.src eq 1.2.3.4', action: 'block' }];
	const ok = (result, status = 200) => Response.json({ success: true, errors: [], result }, { status });
	const no = (message, status) => Response.json({ success: false, errors: [{ code: 1, message }] }, { status });
	globalThis.fetch = async (url, init = {}) => {
		const u = new URL(url);
		const method = init.method ?? 'GET';
		const body = init.body ? JSON.parse(init.body) : null;
		state.calls.push(`${method} ${u.pathname}${u.search}`);
		if (init.headers?.authorization !== `Bearer ${token}`) return no('Invalid API Token', 403);
		const path = u.pathname.replace('/client/v4', '');
		if (path === '/user/tokens/verify') return ok({ status: 'active' });
		if (path === '/zones') return ok(u.searchParams.get('name') === 'example.com' ? [{ id: 'z1', name: 'example.com' }] : []);
		if (path === '/zones/z1') return ok({ id: 'z1', name: 'example.com' });
		const entry = '/zones/z1/rulesets/phases/http_request_firewall_custom/entrypoint';
		if (path === entry && method === 'GET') {
			if (!state.ruleset && state.pending) state.ruleset = { id: 'rs1', rules: state.pending };
			return state.ruleset ? ok(state.ruleset) : no('could not find entrypoint ruleset', 404);
		}
		if (path === entry && method === 'PUT') {
			state.ruleset = { id: 'rs1', rules: body.rules.map((r) => ({ ...r, id: `r${state.nextId++}` })) };
			return ok(state.ruleset);
		}
		if (path === '/zones/z1/rulesets/rs1/rules' && method === 'POST') {
			state.ruleset.rules.push({ ...body, id: `r${state.nextId++}` });
			return ok(state.ruleset);
		}
		const m = /^\/zones\/z1\/rulesets\/rs1\/rules\/(.+)$/.exec(path);
		if (m && method === 'PATCH') {
			state.ruleset.rules = state.ruleset.rules.map((r) => (r.id === m[1] ? { ...r, ...body } : r));
			return ok(state.ruleset);
		}
		if (m && method === 'DELETE') {
			state.ruleset.rules = state.ruleset.rules.filter((r) => r.id !== m[1]);
			return ok(state.ruleset);
		}
		return no(`unexpected ${method} ${path}`, 400);
	};
	return state;
}

const realFetch = globalThis.fetch;
const saved = {};
beforeEach(() => {
	for (const k of ['CMS_SECRET', 'CMS_PUBLIC_URL', 'CLOUDFLARE_API_TOKEN', 'CLOUDFLARE_ZONE_ID']) saved[k] = process.env[k];
	// A fresh random key per test: nothing secret-looking is committed.
	process.env.CMS_SECRET = `test-${crypto.randomUUID()}`;
	process.env.CMS_PUBLIC_URL = 'https://www.example.com';
	delete process.env.CLOUDFLARE_API_TOKEN;
	delete process.env.CLOUDFLARE_ZONE_ID;
});
afterEach(() => {
	globalThis.fetch = realFetch;
	for (const [k, v] of Object.entries(saved)) (v === undefined ? delete process.env[k] : (process.env[k] = v));
});
const ruleRefs = (state) => state.ruleset.rules.map((r) => r.ref).sort();

describe('expressions', () => {
	test('paths: scoped to the hosts, wildcard per pattern, split under the length limit', () => {
		const [r] = pathRules(['/wp-admin*', '*.php', '/a"b'], ['www.example.com']);
		expect(r.expression).toBe('http.host in {"www.example.com"} and (http.request.uri.path wildcard "*.php" or http.request.uri.path wildcard "/a\\"b" or http.request.uri.path wildcard "/wp-admin*")');
		const many = pathRules(Array.from({ length: 200 }, (_, i) => `/probe-number-${i}*`), ['www.example.com']);
		expect(many.length).toBeGreaterThan(1);
		expect(many.every((x) => x.expression.length <= MAX_EXPRESSION)).toBe(true);
		expect(many.map((x) => x.ref)).toEqual(many.map((_, i) => `norns_cms_paths_${i + 1}`));
		expect(pathRules([], ['www.example.com'])).toEqual([]);
	});

	test('countries: allow / block, unknown, team paths and cron', () => {
		const base = { countries: ['DE', 'RO'], unknown: 'allow', admin: false };
		const allow = countryRule({ ...base, mode: 'allow' }, ['www.example.com']).expression;
		expect(allow).toContain('(not ip.src.country in {"DE" "RO"} and not ip.src.country eq "XX")');
		expect(allow).toContain('not http.request.uri.path wildcard "/api/cron*"');
		expect(allow).toContain('http.request.uri.path wildcard "/admin*"');
		const block = countryRule({ ...base, mode: 'block', unknown: 'block', admin: true }, ['www.example.com']).expression;
		expect(block).toContain('(ip.src.country in {"DE" "RO"} or ip.src.country eq "XX")');
		expect(block).not.toContain('/admin*');
		expect(countryRule({ ...base, mode: 'off' }, ['x'])).toBeNull();
		expect(zoneCandidates('www.example.co.uk')).toEqual(['www.example.co.uk', 'example.co.uk', 'co.uk']);
	});
});

describe('sync with Cloudflare', () => {
	test('a token is checked and stored sealed; rules are created next to existing ones and follow changes', async () => {
		const c = await bootApp();
		const cf = fakeCloudflare();
		expect((await cloudflare(c).save({ token: 'bad-token' })).error).toContain('Invalid API Token');
		expect((await cloudflare(c).status()).tokenSaved).toBe(false);

		const r = await cloudflare(c).save({ token: 'good-token', paths: true, countries: true, auto: true });
		expect(r.ok).toBe(true);
		expect(r.status).toMatchObject({ tokenSaved: true, zoneId: 'z1', zoneName: 'example.com', ready: true });
		expect(JSON.stringify(r.status)).not.toContain('good-token');
		const raw = await c.resolve('protection.repo').state('cloudflare');
		expect(raw).not.toContain('good-token');
		expect(r.synced).toMatchObject({ ok: true, zone: 'example.com', hosts: ['www.example.com'] });
		expect(ruleRefs(cf)).toEqual(['my_rule', 'norns_cms_paths']);

		// Automatic: a new pattern updates the rule in place.
		await blocked(c).add({ pattern: '/solr*' });
		const pathsRule = cf.ruleset.rules.find((x) => x.ref === 'norns_cms_paths');
		expect(pathsRule.expression).toContain('"/solr*"');
		expect(pathsRule.description).toContain('managed by the CMS');
		expect(cf.calls.some((x) => x.startsWith('PATCH /client/v4/zones/z1/rulesets/rs1/rules/'))).toBe(true);

		await countryFilter(c).save({ mode: 'block', countries: ['CN'] }, {});
		expect(ruleRefs(cf)).toEqual(['my_rule', 'norns_cms_countries', 'norns_cms_paths']);
		await countryFilter(c).save({ mode: 'off', countries: [] }, {});
		expect(ruleRefs(cf)).toEqual(['my_rule', 'norns_cms_paths']);

		// Removing takes only ours away.
		expect((await cloudflare(c).remove()).ok).toBe(true);
		expect(ruleRefs(cf)).toEqual(['my_rule']);
		expect(cf.ruleset.rules[0].expression).toBe('ip.src eq 1.2.3.4');
	});

	test('a zone without custom rules gets its entrypoint created; switches decide what is mirrored', async () => {
		const c = await bootApp();
		const cf = fakeCloudflare({ preexisting: false });
		await countryFilter(c).save({ mode: 'allow', countries: ['RO'] }, { country: 'RO' });
		const r = await cloudflare(c).save({ token: 'good-token', paths: false, countries: true, auto: false });
		expect(r.synced.ok).toBe(true);
		expect(cf.calls).toContain('PUT /client/v4/zones/z1/rulesets/phases/http_request_firewall_custom/entrypoint');
		expect(ruleRefs(cf)).toEqual(['norns_cms_countries']);
		const before = cf.calls.length;
		await blocked(c).add({ pattern: '/jenkins*' }); // auto sync off
		expect(cf.calls.length).toBe(before);
	});

	test('without CMS_SECRET no token is stored; the env token wins; errors are kept for the panel', async () => {
		const c = await bootApp();
		delete process.env.CMS_SECRET;
		expect((await cloudflare(c).save({ token: 'good-token' })).error).toContain('CMS_SECRET');
		process.env.CLOUDFLARE_API_TOKEN = 'good-token';
		fakeCloudflare({ token: 'other-token' });
		const r = await cloudflare(c).sync();
		expect(r.ok).toBe(false);
		expect((await cloudflare(c).status())).toMatchObject({ tokenFromEnv: true, ready: true, lastError: 'Invalid API Token' });
		fakeCloudflare();
		expect((await cloudflare(c).sync()).ok).toBe(true);
		expect((await cloudflare(c).status()).lastError).toBeNull();
	});

	test('a site whose hosts are not on the zone is refused', async () => {
		const c = await bootApp();
		process.env.CMS_PUBLIC_URL = 'https://www.other.org';
		process.env.CLOUDFLARE_ZONE_ID = 'z1';
		process.env.CLOUDFLARE_API_TOKEN = 'good-token';
		fakeCloudflare();
		expect((await cloudflare(c).sync()).error).toContain('example.com');
	});
});
