import { describe, test, expect, afterEach } from 'bun:test';
import { withScope } from '@human-synthesis/norns/server';
import { bootApp } from './helpers.js';
import { cms } from '../src/lib/norns/content/server/public.c';
import { seo, seoHandle } from '../src/lib/norns/seo/server/public.c';
import { settings } from '../src/lib/norns/settings/server/public.c';
import { i18n } from '../src/lib/norns/i18n/server/public.c';

// Search engine notifications: IndexNow on content changes, Google Search
// Console sitemap submission, the cron catch-up for scheduled entries.

const realFetch = globalThis.fetch;
afterEach(() => {
	globalThis.fetch = realFetch;
	delete process.env.GOOGLE_SERVICE_ACCOUNT;
});

/** Records outgoing requests; `reply(url, init)` → Response. */
function stubFetch(reply = () => new Response('', { status: 200 })) {
	const calls = [];
	globalThis.fetch = async (url, init = {}) => {
		const body = typeof init.body === 'string' ? init.body : init.body?.toString();
		calls.push({ url: String(url), method: init.method ?? 'GET', body, headers: init.headers ?? {} });
		return reply(String(url), init);
	};
	return calls;
}
const indexNowCalls = (calls) => calls.filter((x) => x.url.startsWith('https://api.indexnow.org')).map((x) => JSON.parse(x.body));

/** Runs fn inside a request scope on `url` (no waitUntil: flushes inline). */
async function at(c, url, fn) {
	const u = new URL(url);
	const event = { url: u, locals: { container: c }, request: new Request(u), cookies: { get: () => undefined }, platform: {} };
	return withScope({ container: c, event }, async () => {
		const svc = c.resolve('i18n.service');
		svc.invalidate(); // the primary host comes from the request here
		event.locals.i18nConfig = await i18n(c).config();
		event.locals.locale = svc.describe(svc.resolve(u.host, u.pathname, ''));
		return fn();
	});
}

const SITE = 'https://www.example.com';
const save = (c, type, raw) => cms(c).save(type, null, raw, null).then((r) => r.entry);

describe('IndexNow', () => {
	test('publishing, editing, moving and unpublishing announce the URLs; the key file is served', async () => {
		const c = await bootApp();
		const calls = stubFetch();
		await at(c, `${SITE}/admin`, async () => {
			const draft = await save(c, 'article', { title: 'Launch notes' });
			expect(indexNowCalls(calls)).toHaveLength(0); // drafts are private

			await cms(c).setStatus('article', draft.id, 'published');
			let [sent] = indexNowCalls(calls);
			expect(sent.host).toBe('www.example.com');
			expect(sent.key).toMatch(/^[0-9a-f]{32}$/);
			expect(sent.keyLocation).toBe(`${SITE}/${sent.key}.txt`);
			expect(sent.urlList.sort()).toEqual([`${SITE}/articles`, `${SITE}/articles/launch-notes`]);
			const handle = seoHandle();
			const get = (path) => {
				const url = new URL(path, SITE);
				return handle({ event: { url, request: new Request(url), locals: { container: c, locale: { rest: url.pathname } } }, resolve: async () => new Response('', { status: 404 }) });
			};
			const keyFile = await get(`/${sent.key}.txt`);
			expect(keyFile.status).toBe(200);
			expect(await keyFile.text()).toBe(sent.key);
			expect((await get('/0123456789abcdef0123456789abcdef.txt')).status).toBe(404);

			// A new slug: the old URL (now a redirect) and the new one.
			await cms(c).save('article', draft.id, { title: 'Launch notes', slug: 'launch' }, null);
			sent = indexNowCalls(calls).at(-1);
			expect(sent.urlList).toContain(`${SITE}/articles/launch`);
			expect(sent.urlList).toContain(`${SITE}/articles/launch-notes`);

			await cms(c).setStatus('article', draft.id, 'draft');
			expect(indexNowCalls(calls).at(-1).urlList).toContain(`${SITE}/articles/launch`);
			const log = (await seo(c).indexingStatus()).log;
			expect(log[0]).toMatchObject({ engine: 'indexnow', target: 'www.example.com', ok: 1, status: 200 });
		});
	});

	test('nothing is sent from dev hosts, for members-only entries, or when switched off', async () => {
		const c = await bootApp();
		const calls = stubFetch();
		await at(c, 'http://localhost:5173/admin', async () => {
			const e = await save(c, 'article', { title: 'Local' });
			await cms(c).setStatus('article', e.id, 'published');
		});
		await at(c, `${SITE}/admin`, async () => {
			const m = await save(c, 'article', { title: 'Members', visibility: 'members' });
			await cms(c).setStatus('article', m.id, 'published');
			await seo(c).saveIndexingRules({ enabled: false });
			const e = await save(c, 'article', { title: 'Quiet' });
			await cms(c).setStatus('article', e.id, 'published');
		});
		expect(indexNowCalls(calls)).toHaveLength(0);
	});

	test('per type rules choose what goes out by itself; the sitemap still counts as changed', async () => {
		const c = await bootApp();
		process.env.GOOGLE_SERVICE_ACCOUNT = '{}';
		const calls = stubFetch();
		await at(c, `${SITE}/admin`, async () => {
			const rules = await seo(c).saveIndexingRules({ auto: { article: { new: false, updated: true }, page: { removed: false } } });
			expect(rules.auto.article).toMatchObject({ label: 'Articles', new: false, updated: true, removed: true });
			expect(rules.auto.page).toMatchObject({ new: true, updated: true, removed: false });

			const a = await save(c, 'article', { title: 'Quiet launch' });
			await cms(c).setStatus('article', a.id, 'published'); // new: off
			expect(indexNowCalls(calls)).toHaveLength(0);
			expect((await seo(c).indexingStatus()).googlePending).toBe(true);
			await cms(c).save('article', a.id, { title: 'Quiet launch', summary: 'More.' }, null); // updated: on
			expect(indexNowCalls(calls).at(-1).urlList).toContain(`${SITE}/articles/quiet-launch`);

			const p = await save(c, 'page', { title: 'Old', layout: 'markdown', body: 'x' });
			await cms(c).setStatus('page', p.id, 'published'); // new: on
			expect(indexNowCalls(calls).at(-1).urlList).toEqual([`${SITE}/old`]);
			const before = indexNowCalls(calls).length;
			await cms(c).remove('page', p.id); // removed: off
			expect(indexNowCalls(calls)).toHaveLength(before);
		});
	});

	test('by hand: one entry whatever the rules say, or chosen URLs on this site only', async () => {
		const c = await bootApp();
		const calls = stubFetch();
		await at(c, `${SITE}/admin`, async () => {
			await seo(c).saveIndexingRules({ auto: { article: { new: false, updated: false, removed: false } } });
			const a = await save(c, 'article', { title: 'Manual' });
			const ct = cms(c).getType('article');
			expect((await seo(c).submitEntry(ct, a)).reason).toContain('published');
			await cms(c).setStatus('article', a.id, 'published');
			expect(indexNowCalls(calls)).toHaveLength(0);
			const r = await seo(c).submitEntry(ct, await cms(c).get('article', a.id));
			expect(r.sent).toBe(2);
			expect(indexNowCalls(calls)[0].urlList).toContain(`${SITE}/articles/manual`);

			const u = await seo(c).submitUrls(`${SITE}/pricing\nhttps://other.example.org/x\n  \nhttp://www.example.com/insecure`);
			expect(u.sent).toBe(1);
			expect(u.skipped).toEqual(['https://other.example.org/x', 'http://www.example.com/insecure']);
			expect(indexNowCalls(calls)[1].urlList).toEqual([`${SITE}/pricing`]);

			await seo(c).saveIndexingRules({ enabled: false });
			expect((await seo(c).submitUrls([`${SITE}/pricing`])).reason).toContain('off');
			expect((await seo(c).submitAll()).reason).toContain('off');
		});
	});

	test('rate limits keep the URLs queued for the next run', async () => {
		const c = await bootApp();
		stubFetch(() => new Response('slow down', { status: 429 }));
		await at(c, `${SITE}/admin`, async () => {
			const e = await save(c, 'page', { title: 'About', layout: 'markdown', body: 'Hi' });
			await cms(c).setStatus('page', e.id, 'published');
			const status = await seo(c).indexingStatus();
			expect(status.queued).toBe(1);
			expect(status.log[0]).toMatchObject({ ok: 0, status: 429 });
			const calls = stubFetch();
			await seo(c).indexingTick();
			expect(indexNowCalls(calls)[0].urlList).toEqual([`${SITE}/about`]);
			expect((await seo(c).indexingStatus()).queued).toBe(0);
		});
	});

	test('the cron announces scheduled entries once they go live', async () => {
		const c = await bootApp();
		const calls = stubFetch();
		await at(c, `${SITE}/admin`, async () => {
			await seo(c).indexingTick(); // first run: remembers "now"
			const e = await save(c, 'article', { title: 'Later' });
			await cms(c).setStatus('article', e.id, 'published', { publishAt: Date.now() + 30 });
			expect(indexNowCalls(calls)).toHaveLength(0);
			await Bun.sleep(60);
			const r = await seo(c).indexingTick();
			expect(r.scheduled).toBe(1);
			expect(indexNowCalls(calls)[0].urlList).toContain(`${SITE}/articles/later`);
			expect((await seo(c).indexingTick()).scheduled).toBe(0);
		});
	});

	test('submit all sends every public, indexable URL with the language homes', async () => {
		const c = await bootApp();
		await at(c, `${SITE}/admin`, async () => {
			await seo(c).saveIndexingRules({ enabled: false });
			for (const title of ['One', 'Two']) {
				const e = await save(c, 'article', { title });
				await cms(c).setStatus('article', e.id, 'published');
			}
			const hidden = await save(c, 'article', { title: 'Hidden', seoNoindex: 'on' });
			await cms(c).setStatus('article', hidden.id, 'published');
			await seo(c).saveIndexingRules({ enabled: true });
			const calls = stubFetch();
			const r = await seo(c).submitAll();
			const [sent] = indexNowCalls(calls);
			expect(r.sent).toBe(4);
			expect(sent.urlList.sort()).toEqual([`${SITE}/`, `${SITE}/articles`, `${SITE}/articles/one`, `${SITE}/articles/two`]);
		});
	});
});

// PEM armour around DER bytes; the label is passed in so the key pair is
// clearly generated here, per test run.
function toPem(label, der) {
	const body = btoa(String.fromCharCode(...der)).replace(/.{64}/g, '$&\n');
	return `-----BEGIN ${label}-----\n${body}\n-----END ${label}-----\n`;
}

async function serviceAccount() {
	const pair = await crypto.subtle.generateKey({ name: 'RSASSA-PKCS1-v1_5', modulusLength: 2048, publicExponent: new Uint8Array([1, 0, 1]), hash: 'SHA-256' }, true, ['sign', 'verify']);
	const der = new Uint8Array(await crypto.subtle.exportKey('pkcs8', pair.privateKey));
	const pem = toPem('PRIVATE KEY', der);
	return { json: JSON.stringify({ client_email: 'cms@proj.iam.gserviceaccount.com', private_key: pem }), publicKey: pair.publicKey };
}

describe('Google Search Console', () => {
	test('a change marks the sitemap; the cron signs in with the service account and resubmits it', async () => {
		const c = await bootApp();
		const sa = await serviceAccount();
		process.env.GOOGLE_SERVICE_ACCOUNT = sa.json;
		const calls = stubFetch((url) => (url.includes('oauth2') ? Response.json({ access_token: 'tok' }) : new Response(null, { status: 204 })));
		await at(c, `${SITE}/admin`, async () => {
			expect((await seo(c).indexingTick()).google).toBeUndefined(); // nothing changed yet
			const e = await save(c, 'article', { title: 'Indexed' });
			await cms(c).setStatus('article', e.id, 'published');
			expect((await seo(c).indexingStatus()).googlePending).toBe(true);
			const r = await seo(c).indexingTick();
			expect(r.google.ok).toBe(true);

			const token = calls.find((x) => x.url === 'https://oauth2.googleapis.com/token');
			const assertion = new URLSearchParams(token.body).get('assertion');
			const [h, p, s] = assertion.split('.');
			const claims = JSON.parse(atob(p.replace(/-/g, '+').replace(/_/g, '/')));
			expect(claims).toMatchObject({ iss: 'cms@proj.iam.gserviceaccount.com', scope: 'https://www.googleapis.com/auth/webmasters', aud: 'https://oauth2.googleapis.com/token' });
			const sig = Uint8Array.from(atob(s.replace(/-/g, '+').replace(/_/g, '/')), (ch) => ch.charCodeAt(0));
			expect(await crypto.subtle.verify('RSASSA-PKCS1-v1_5', sa.publicKey, sig, new TextEncoder().encode(`${h}.${p}`))).toBe(true);

			const put = calls.find((x) => x.method === 'PUT');
			expect(put.url).toBe(`https://www.googleapis.com/webmasters/v3/sites/${encodeURIComponent(`${SITE}/`)}/sitemaps/${encodeURIComponent(`${SITE}/sitemap.xml`)}`);
			expect(put.headers.authorization).toBe('Bearer tok');
			const status = await seo(c).indexingStatus();
			expect(status.googlePending).toBe(false);
			expect(status.googleAt).toBeGreaterThan(0);
		});
	});

	test('a domain property is used as set; a refusal names the account to add', async () => {
		const c = await bootApp();
		process.env.GOOGLE_SERVICE_ACCOUNT = (await serviceAccount()).json;
		const calls = stubFetch((url) => (url.includes('oauth2') ? Response.json({ access_token: 'tok' }) : Response.json({ error: { message: 'User does not have sufficient permission' } }, { status: 403 })));
		await at(c, `${SITE}/admin`, async () => {
			await settings(c).save('seo', { ...(await settings(c).get('seo')), gscProperty: 'sc-domain:example.com' }, null);
			const r = await seo(c).submitSitemaps();
			expect(r.ok).toBe(false);
			expect(calls.find((x) => x.method === 'PUT').url).toContain(`/sites/${encodeURIComponent('sc-domain:example.com')}/`);
			expect((await seo(c).indexingStatus()).log[0].message).toContain('add cms@proj.iam.gserviceaccount.com as an owner');
		});
	});

	test('without the secret nothing is sent; a broken secret is logged', async () => {
		const c = await bootApp();
		const calls = stubFetch();
		await at(c, `${SITE}/admin`, async () => {
			expect((await seo(c).submitSitemaps()).skipped).toContain('GOOGLE_SERVICE_ACCOUNT');
			process.env.GOOGLE_SERVICE_ACCOUNT = '{not json';
			expect((await seo(c).submitSitemaps()).ok).toBe(false);
			expect((await seo(c).indexingStatus()).log[0]).toMatchObject({ engine: 'google', ok: 0, message: 'GOOGLE_SERVICE_ACCOUNT is not valid JSON' });
		});
		expect(calls).toHaveLength(0);
	});
});
