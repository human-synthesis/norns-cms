import { describe, test, expect } from 'bun:test';
import { bootApp } from './helpers.js';
import { auth } from '../src/lib/norns/auth/server/public.c';
import { apiKeys, handleApi, endpoints, openapiDoc } from '../src/lib/norns/api/server/public.c';

const PW = 'correct horse battery';

async function setup() {
	const c = await bootApp();
	const ownerId = await auth(c).createUser('owner@example.com', 'Olga', PW, 'owner');
	const owner = { ...(await auth(c).getUser(ownerId)), capabilities: ['*'] };
	return { c, owner };
}

// A request through the real handler, the way the /api/v1 route calls it.
function call(c, key, method, path, body, headers = {}) {
	const url = new URL(`http://localhost/api/v1${path}`);
	const init = { method, headers: { ...(key ? { authorization: `Bearer ${key}` } : {}), ...headers } };
	if (body !== undefined) {
		init.body = body instanceof Uint8Array ? body : JSON.stringify(body);
		if (!(body instanceof Uint8Array)) init.headers['content-type'] = 'application/json';
	}
	const event = { request: new Request(url, init), url, locals: { container: c }, cookies: { get: () => undefined }, getClientAddress: () => '127.0.0.1' };
	return handleApi(event, url.pathname.slice('/api/v1'.length)).then(async (r) => ({ status: r.status, body: r.status === 204 ? null : await r.json(), headers: r.headers }));
}

describe('keys', () => {
	test('a key is shown once, stored hashed, and acts for its creator', async () => {
		const { c, owner } = await setup();
		const r = await apiKeys(c).create({ name: 'Sync', capabilities: ['content.view'] }, owner);
		expect(r.key).toMatch(/^ncms_[a-z0-9]{8}_/);
		const [row] = await apiKeys(c).list();
		expect(JSON.stringify(row)).not.toContain(r.key);
		const me = await call(c, r.key, 'GET', '/me');
		expect(me.status).toBe(200);
		expect(me.body.data.user.email).toBe('owner@example.com');
		expect(me.body.data.capabilities).toEqual(['content.view']);
	});

	test('no key, a bad key, a revoked or expired key → 401', async () => {
		const { c, owner } = await setup();
		expect((await call(c, null, 'GET', '/me')).status).toBe(401);
		expect((await call(c, 'ncms_aaaaaaaa_' + 'x'.repeat(32), 'GET', '/me')).status).toBe(401);
		const r = await apiKeys(c).create({ name: 'Old', capabilities: ['content.view'] }, owner);
		await apiKeys(c).revoke(r.id, owner);
		expect((await call(c, r.key, 'GET', '/me')).status).toBe(401);
	});

	test('a key cannot exceed its creator, now or later', async () => {
		const { c, owner } = await setup();
		const editorId = await auth(c).createUser('ed@example.com', 'Ed', PW, 'editor');
		const editor = { ...(await auth(c).getUser(editorId)), capabilities: (await auth(c).role('editor')).capabilities };
		expect((await apiKeys(c).create({ name: 'X', capabilities: ['users.manage'] }, editor)).ok).toBe(false);
		expect((await apiKeys(c).create({ name: 'X', capabilities: ['*'] }, editor)).ok).toBe(false);
		const r = await apiKeys(c).create({ name: 'Ed key', capabilities: ['content.publish', 'content.view'] }, editor);
		expect(r.ok).toBe(true);
		expect((await call(c, r.key, 'GET', '/users')).status).toBe(403);
		await auth(c).updateUser(editorId, { role: 'author' }, owner);
		expect((await call(c, r.key, 'GET', '/me')).body.data.capabilities).toEqual(['content.view']);
		await auth(c).updateUser(editorId, { status: 'suspended' }, owner);
		expect((await call(c, r.key, 'GET', '/me')).status).toBe(401);
	});
});

describe('requests', () => {
	test('errors: unknown endpoint 404, wrong method 405, bad JSON 400; CORS preflight', async () => {
		const { c, owner } = await setup();
		const { key } = await apiKeys(c).create({ name: 'K', capabilities: ['*'] }, owner);
		expect((await call(c, key, 'GET', '/nope')).status).toBe(404);
		expect((await call(c, key, 'PUT', '/me')).status).toBe(405);
		const url = new URL('http://localhost/api/v1/content/article');
		const bad = await handleApi({ request: new Request(url, { method: 'POST', headers: { authorization: `Bearer ${key}`, 'content-type': 'application/json' }, body: '{nope' }), url, locals: { container: c }, cookies: { get: () => undefined } }, '/content/article');
		expect(bad.status).toBe(400);
		const pre = await call(c, null, 'OPTIONS', '/content/article');
		expect(pre.status).toBe(204);
		expect(pre.headers.get('access-control-allow-origin')).toBe('*');
	});

	test('content: create, validate, patch, publish, list, delete', async () => {
		const { c, owner } = await setup();
		const { key } = await apiKeys(c).create({ name: 'K', capabilities: ['*'] }, owner);
		const missing = await call(c, key, 'POST', '/content/article', { data: { body: 'x' } });
		expect(missing.status).toBe(400);
		expect(missing.body.error.issues[0]).toEqual({ field: 'title', message: 'Title is required' });
		const created = await call(c, key, 'POST', '/content/article', { data: { title: 'Via API', body: '## Hi', tags: ['api'] } });
		expect(created.status).toBe(201);
		const id = created.body.data.id;
		expect(created.body.data.status).toBe('draft');
		expect(created.body.data.author_id).toBe(owner.id);
		const patched = await call(c, key, 'PATCH', `/content/article/${id}`, { data: { summary: 'Short.' } });
		expect(patched.body.data.data.title).toBe('Via API');
		expect(patched.body.data.data.summary).toBe('Short.');
		const pub = await call(c, key, 'POST', `/content/article/${id}/status`, { status: 'published' });
		expect(pub.body.data.status).toBe('published');
		const one = await call(c, key, 'GET', `/content/article/${id}`);
		expect(one.body.data.html.body).toContain('<h2 id="hi">');
		const list = await call(c, key, 'GET', '/content/article?status=published');
		expect(list.body.meta.total).toBe(1);
		expect((await call(c, key, 'GET', '/content/post')).status).toBe(404);
		expect((await call(c, key, 'DELETE', `/content/article/${id}`)).body.data).toEqual({ deleted: id });
	});

	test('authors keep author rules through the API', async () => {
		const { c, owner } = await setup();
		const aId = await auth(c).createUser('au@example.com', 'Au', PW, 'author');
		const author = { ...(await auth(c).getUser(aId)), capabilities: (await auth(c).role('author')).capabilities };
		const { key } = await apiKeys(c).create({ name: 'A', capabilities: ['content.view', 'content.create', 'content.edit_own'] }, author);
		const mine = await call(c, key, 'POST', '/content/article', { data: { title: 'Mine' } });
		expect(mine.status).toBe(201);
		expect((await call(c, key, 'POST', `/content/article/${mine.body.data.id}/status`, { status: 'published' })).status).toBe(403);
		expect((await call(c, key, 'POST', `/content/article/${mine.body.data.id}/status`, { status: 'in_review' })).status).toBe(200);
		const { key: ok } = await apiKeys(c).create({ name: 'O', capabilities: ['*'] }, owner);
		const theirs = await call(c, ok, 'POST', '/content/article', { data: { title: 'Owner piece' } });
		expect((await call(c, key, 'PATCH', `/content/article/${theirs.body.data.id}`, { data: { title: 'Hijack' } })).status).toBe(403);
	});

	test('media upload takes raw bytes', async () => {
		const { c, owner } = await setup();
		const { key } = await apiKeys(c).create({ name: 'M', capabilities: ['media.upload', 'media.view'] }, owner);
		const png = new Uint8Array(33);
		png.set([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0, 0, 0, 13, 0x49, 0x48, 0x44, 0x52]);
		png.set([0, 0, 0, 64, 0, 0, 0, 32], 16);
		const up = await call(c, key, 'POST', '/media?filename=dot.png&alt=Dot', png, { 'content-type': 'image/png' });
		expect(up.status).toBe(201);
		expect(up.body.data.width).toBe(64);
		expect((await call(c, key, 'POST', '/media', png, { 'content-type': 'image/png' })).status).toBe(400);
	});

	test('users, settings, SEO and consent', async () => {
		const { c, owner } = await setup();
		const { key } = await apiKeys(c).create({ name: 'All', capabilities: ['*'] }, owner);
		const invited = await call(c, key, 'POST', '/users', { email: 'new@example.com', name: 'New', role: 'author' });
		expect(invited.body.data.status).toBe('invited');
		expect(invited.body.data.inviteLink).toContain('/invite/');
		expect((await call(c, key, 'PATCH', `/users/${owner.id}`, { role: 'editor' })).status).toBe(400);
		const s = await call(c, key, 'PATCH', '/settings/site', { title: 'API site' });
		expect(s.body.data.title).toBe('API site');
		expect((await call(c, key, 'PATCH', '/settings/site', { nope: 1 })).status).toBe(400);
		expect((await call(c, key, 'POST', '/seo/redirects', { from: '/a', to: '/b' })).status).toBe(201);
		expect((await call(c, key, 'POST', '/seo/redirects', { from: '/b', to: '/a' })).status).toBe(400);
		const script = await call(c, key, 'POST', '/consent/scripts', { provider: 'ga4', config: { measurementId: 'G-ABCDE123' } });
		expect(script.body.data.config.measurementId).toBe('G-ABCDE123');
		expect((await call(c, key, 'PATCH', `/consent/scripts/${script.body.data.id}`, { enabled: false })).body.data.enabled).toBe(false);
		expect((await call(c, key, 'POST', '/consent/version')).body.data.policyVersion).toBe(2);
		const audit = await call(c, key, 'GET', '/audit?action=consent.');
		expect(audit.body.data[0].actor).toBe('owner@example.com (API key "All")');
	});
});

describe('documentation', () => {
	test('every endpoint is documented and appears in OpenAPI', async () => {
		const { c } = await setup();
		const eps = endpoints(c);
		expect(eps.length).toBeGreaterThan(40);
		for (const ep of eps) {
			expect(ep.summary, `${ep.method} ${ep.path}`).toBeTruthy();
			expect(ep.tag, `${ep.method} ${ep.path}`).toBeTruthy();
			expect(ep.capability === null || typeof ep.capability === 'string').toBe(true);
		}
		const doc = await openapiDoc(c);
		expect(doc.paths['/content/{type}/{id}'].patch.summary).toBeTruthy();
		expect(Object.keys(doc.paths).length).toBeGreaterThan(25);
	});
});
