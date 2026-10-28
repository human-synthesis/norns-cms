import { describe, test, expect, afterEach } from 'bun:test';
import { readdirSync, readFileSync, statSync } from 'node:fs';
import { join, relative } from 'node:path';
import { bootApp } from './helpers.js';
import { auth } from '../src/lib/norns/auth/server/public.c';
import { protection, protect } from '../src/lib/norns/protection/server/public.c';
import { signValue } from '../src/lib/norns/settings/server/public.c';
import { apiKeys, handleApi } from '../src/lib/norns/api/server/public.c';

const PW = 'correct horse battery';
const realFetch = globalThis.fetch;
afterEach(() => {
	globalThis.fetch = realFetch;
	delete process.env.TURNSTILE_ENABLED;
});

const tokenAt = (ms) => `${ms}.${signValue(String(ms), 'form-token').slice(0, 32)}`;
const fdOf = (o) => {
	const fd = new FormData();
	for (const [k, v] of Object.entries(o)) fd.set(k, v);
	return fd;
};
const eventFor = (c, path = '/login', lang = 'en') => ({ url: new URL(`http://localhost${path}`), locals: { container: c, locale: { language: lang } }, getClientAddress: () => '9.9.9.9' });
// Cloudflare siteverify stand-in: answers by token.
const fakeCloudflare = (calls = []) => {
	globalThis.fetch = async (url, init) => {
		const body = new URLSearchParams(String(init.body));
		calls.push(Object.fromEntries(body));
		if (body.get('secret') === 'bad-secret') return Response.json({ success: false, 'error-codes': ['invalid-input-secret'] });
		const token = body.get('response');
		if (token === 'down') throw new Error('offline');
		if (token?.startsWith('ok:')) return Response.json({ success: true, action: token.slice(3), hostname: 'localhost', 'error-codes': [] });
		return Response.json({ success: false, 'error-codes': ['invalid-input-response'] });
	};
	return calls;
};
const owner = async (c) => {
	const id = await auth(c).createUser('o@example.com', 'O', PW, 'owner');
	return { ...(await auth(c).getUser(id)), capabilities: ['*'] };
};

describe('registry', () => {
	test('every public form is registered: accounts, two-factor steps and each form in Forms', async () => {
		const c = await bootApp();
		const ids = (await protection(c).definitions()).map((d) => d.id);
		for (const id of ['auth.staff-login', 'auth.staff-forgot', 'auth.reset', 'auth.invite', 'auth.member-login', 'auth.member-resend', 'auth.member-register', 'auth.member-forgot', 'twofactor.staff-step', 'twofactor.member-step', 'forms:contact', 'forms:newsletter']) {
			expect(ids).toContain(id);
		}
	});

	test('every public route with form actions is wrapped in protect()', () => {
		const ROUTES = join(import.meta.dir, '..', 'src', 'routes');
		// Signed-in pages: the visitor is already authenticated.
		const SIGNED_IN = ['(site)/account/+page.server.c', '(site)/account/privacy/+page.server.c'];
		const walk = (d, out = []) => {
			for (const n of readdirSync(d)) {
				const f = join(d, n);
				if (statSync(f).isDirectory()) walk(f, out);
				else if (n === '+page.server.c') out.push(f);
			}
			return out;
		};
		const missing = walk(ROUTES)
			.map((f) => relative(ROUTES, f))
			.filter((f) => !f.startsWith('admin/') && !SIGNED_IN.includes(f))
			.filter((f) => /page\.actions/.test(readFileSync(join(ROUTES, f), 'utf8')) && !/export actions := protect /.test(readFileSync(join(ROUTES, f), 'utf8')));
		expect(missing).toEqual([]);
		expect(readFileSync(join(ROUTES, 'forms', '[slug]', '+server.c'), 'utf8')).toContain('protection(c).check');
	});
});

describe('honeypot and render time', () => {
	test('filled honeypot, missing or forged token, too fast; a person passes; outcomes are counted', async () => {
		const c = await bootApp();
		const ev = eventFor(c, '/forms/contact');
		const old = tokenAt(Date.now() - 10_000);
		expect(await protection(c).check(ev, 'forms:contact', fdOf({ _hp: 'http://spam', _g: old }))).toMatchObject({ ok: false, reason: 'honeypot', silent: true });
		expect((await protection(c).check(ev, 'forms:contact', fdOf({}))).reason).toBe('token');
		expect((await protection(c).check(ev, 'forms:contact', fdOf({ _g: `${Date.now() - 10_000}.deadbeef` }))).reason).toBe('token');
		expect((await protection(c).check(ev, 'forms:contact', fdOf({ _g: tokenAt(Date.now() - 4 * 86400_000) }))).reason).toBe('token');
		expect(await protection(c).check(ev, 'forms:contact', fdOf({ _g: tokenAt(Date.now() - 500) }))).toMatchObject({ ok: false, reason: 'too_fast', silent: true });
		expect((await protection(c).check(ev, 'forms:contact', fdOf({ _g: old }))).ok).toBe(true);
		// Sign-in may be instant (password managers).
		expect((await protection(c).check(eventFor(c), 'auth.staff-login', fdOf({ _g: tokenAt(Date.now()) }))).ok).toBe(true);
		const contact = (await protection(c).forms()).find((f) => f.id === 'forms:contact');
		expect(contact.counts).toEqual({ honeypot: 1, token: 3, too_fast: 1, passed: 1 });
		expect((await protection(c).clientConfig()).token).toMatch(/^\d+\.[0-9a-f]{32}$/);
	});

	test('a form with its honeypot switched off skips it', async () => {
		const c = await bootApp();
		const o = await owner(c);
		expect((await protection(c).saveForm('auth.member-login', { honeypot: false }, o)).ok).toBe(true);
		expect((await protection(c).check(eventFor(c, '/account/login'), 'auth.member-login', fdOf({ _hp: 'x' }))).ok).toBe(true);
	});
});

describe('Turnstile', () => {
	test('keys: both needed to switch on; the secret is checked with Cloudflare, stored sealed, never returned', async () => {
		const c = await bootApp();
		const o = await owner(c);
		process.env.CMS_SECRET = 'test-cms-secret';
		fakeCloudflare();
		expect((await protection(c).saveConfig({ enabled: true, siteKey: '0xSITE' }, o)).field).toBe('secretKey');
		expect((await protection(c).saveConfig({ siteKey: '0xSITE', secretKey: 'bad-secret' }, o)).error).toContain('does not recognise');
		expect((await protection(c).saveConfig({ enabled: true, siteKey: '0xSITE', secretKey: '0xSECRET-good-1234' }, o)).ok).toBe(true);
		const cfg = await protection(c).config();
		expect(cfg).toMatchObject({ active: true, siteKey: '0xSITE', secretSet: true, secretHint: '…1234' });
		expect(cfg.verifiedAt).toBeGreaterThan(0);
		expect(JSON.stringify(cfg)).not.toContain('0xSECRET');
		const row = await c.resolve('db').get('SELECT secret FROM protection_config', []);
		expect(row.secret).toStartWith('v1:');
		expect(row.secret).not.toContain('0xSECRET');
		// Keeping the secret: an empty secret field changes nothing.
		expect((await protection(c).saveConfig({ siteKey: '0xSITE2', secretKey: '' }, o)).ok).toBe(true);
		expect((await protection(c).config()).secretSet).toBe(true);
		delete process.env.CMS_SECRET;
	});

	test('checks the widget token and its action; defaults, overrides, kill switch, Cloudflare down', async () => {
		const c = await bootApp();
		const o = await owner(c);
		const calls = fakeCloudflare();
		await protection(c).saveConfig({ enabled: true, siteKey: '0xSITE', secretKey: '0xSECRET' }, o);
		const ev = eventFor(c);
		const g = tokenAt(Date.now() - 10_000);
		const check = (id, token) => protection(c).check(ev, id, fdOf({ _g: g, 'cf-turnstile-response': token }));
		expect((await check('auth.staff-login', 'ok:auth_staff-login')).ok).toBe(true);
		expect(calls.at(-1)).toMatchObject({ secret: '0xSECRET', remoteip: '9.9.9.9' });
		expect((await check('auth.staff-login', 'nope')).reason).toBe('turnstile');
		expect((await check('auth.staff-login', 'ok:another_form')).reason).toBe('turnstile');
		expect((await protection(c).check(ev, 'auth.staff-login', fdOf({ _g: g }))).reason).toBe('turnstile');
		expect(await check('auth.staff-login', 'down')).toMatchObject({ ok: true, unverified: true });
		// The code step has Turnstile off by default; an override turns it on.
		expect((await check('twofactor.staff-step', '')).ok).toBe(true);
		await protection(c).saveForm('twofactor.staff-step', { turnstile: 'on' }, o);
		expect((await check('twofactor.staff-step', '')).reason).toBe('turnstile');
		await protection(c).saveForm('auth.staff-login', { turnstile: 'off' }, o);
		expect((await check('auth.staff-login', '')).ok).toBe(true);
		const conf = await protection(c).clientConfig();
		expect(conf).toMatchObject({ active: true, siteKey: '0xSITE' });
		expect(conf.forms['auth.staff-login']).toEqual({ turnstile: false, honeypot: true });
		expect(conf.forms['auth.member-login']).toEqual({ turnstile: true, honeypot: true });
		process.env.TURNSTILE_ENABLED = 'false';
		expect((await protection(c).clientConfig()).siteKey).toBe('');
		expect((await check('auth.member-login', '')).ok).toBe(true);
	});
});

describe('protect()', () => {
	test('runs the action when the check passes; otherwise a 400 with the message in the visitor’s language', async () => {
		const c = await bootApp();
		let ran = 0;
		const actions = protect({ login: async (event) => { ran++; return { body: (await event.request.formData()).get('email') }; } }, { login: 'auth.member-login' });
		const req = (o) => ({ ...eventFor(c, '/account/login', 'de'), request: new Request('http://localhost/account/login?/login', { method: 'POST', body: fdOf(o) }) });
		expect(await actions.login(req({ email: 'a@example.com', _g: tokenAt(Date.now()) }))).toEqual({ body: 'a@example.com' });
		const r = await actions.login(req({ email: 'a@example.com', password: 'secret', _hp: 'bot', _g: tokenAt(Date.now()) }));
		expect(ran).toBe(1);
		expect(r.status).toBe(400);
		expect(r.data.guardForm).toBe('auth.member-login');
		expect(r.data.guard).toContain('Mensch');
		expect(r.data.values).toEqual({ email: 'a@example.com' });
	});
});

describe('API', () => {
	test('config without the secret; forms list and patch', async () => {
		const c = await bootApp();
		const o = await owner(c);
		fakeCloudflare();
		const { key } = await apiKeys(c).create({ name: 'Ops', capabilities: ['protection.manage'] }, o);
		const call = (method, path, body) => {
			const url = new URL(`http://localhost/api/v1${path}`);
			const init = { method, headers: { authorization: `Bearer ${key}` } };
			if (body) {
				init.body = JSON.stringify(body);
				init.headers['content-type'] = 'application/json';
			}
			const event = { request: new Request(url, init), url, locals: { container: c }, cookies: { get: () => undefined }, getClientAddress: () => '127.0.0.1' };
			return handleApi(event, path).then(async (r) => ({ status: r.status, body: await r.json() }));
		};
		const p = await call('PATCH', '/protection', { enabled: true, siteKey: '0xSITE', secretKey: '0xSECRETVALUE' });
		expect(p.status).toBe(200);
		expect(p.body.data.active).toBe(true);
		expect(JSON.stringify(p.body)).not.toContain('0xSECRETVALUE');
		const list = await call('GET', '/protection/forms');
		expect(list.body.data.some((f) => f.id === 'forms:contact')).toBe(true);
		const one = await call('PATCH', `/protection/forms/${encodeURIComponent('forms:contact')}`, { turnstile: 'off' });
		expect(one.status).toBe(200);
		expect(one.body.data).toMatchObject({ id: 'forms:contact', turnstileMode: 'off' });
		expect((await call('PATCH', '/protection/forms/nope', { turnstile: 'on' })).status).toBe(404);
	});
});
