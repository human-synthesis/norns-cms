// The CMS features exercised through the DI container exactly the way
// boot() wires them — no HTTP, no Vite. Each bootApp() gets its own
// in-memory SQLite with the real migrations applied.
import { describe, test, expect } from 'bun:test';
import { readdirSync, readFileSync } from 'node:fs';
import { join } from 'node:path';
import { createApp } from '@human-synthesis/norns/server';
import registerAuth from '../src/lib/norns/auth/server/module.c';
import registerContent from '../src/lib/norns/content/server/module.c';
import registerPages from '../src/lib/norns/pages/server/module.c';
import registerPosts from '../src/lib/norns/posts/server/module.c';
import { cms } from '../src/lib/norns/content/server/public.c';
import { auth } from '../src/lib/norns/auth/server/public.c';

// auth's module reads DATABASE_URL lazily (inside the `db` factory), so
// setting it here, after the imports above have evaluated, is enough.
process.env.DATABASE_URL = 'file::memory:';

function migrate(db) {
	const root = join(import.meta.dir, '..', 'migrations');
	for (const feature of readdirSync(root).sort()) {
		const files = readdirSync(join(root, feature))
			.filter((f) => f.endsWith('.sql'))
			.sort();
		for (const f of files) db.exec(readFileSync(join(root, feature, f), 'utf8'));
	}
}

function bootApp() {
	const app = createApp();
	registerAuth(app);
	registerContent(app);
	registerPages(app);
	registerPosts(app);
	migrate(app.resolve('db'));
	// Per-request scope, like event.locals.container.
	return app.scope();
}

describe('content types', () => {
	test('pages and posts are registered regardless of module order', () => {
		const app = createApp();
		registerPosts(app);
		registerPages(app);
		registerAuth(app);
		registerContent(app);
		migrate(app.resolve('db'));
		const ids = cms(app.scope())
			.types()
			.map((t) => t.id)
			.sort();
		expect(ids).toEqual(['page', 'post']);
	});

	test('admin nav lists dashboard plus one entry per type', () => {
		const nav = cms(bootApp()).adminNav();
		expect(nav[0].href).toBe('/admin');
		expect(nav.map((n) => n.href)).toContain('/admin/content/page');
		expect(nav.map((n) => n.href)).toContain('/admin/content/post');
	});
});

describe('entries', () => {
	test('save() rejects a missing title with a field-level issue', () => {
		const r = cms(bootApp()).save('page', null, { body: 'x' }, null);
		expect(r.ok).toBe(false);
		expect(r.issues[0].path[0].key).toBe('title');
		expect(r.issues[0].message).toBe('Title is required');
	});

	test('save() creates a draft with a slug derived from the title', () => {
		const c = bootApp();
		const r = cms(c).save('page', null, { title: 'Hello, World!', body: '# Hi' }, null);
		expect(r.ok).toBe(true);
		const entry = cms(c).get('page', r.id);
		expect(entry.slug).toBe('hello-world');
		expect(entry.status).toBe('draft');
		expect(entry.data.body).toBe('# Hi');
		expect(entry.data.showInNav).toBe(false);
	});

	test('duplicate slugs get a numeric suffix', () => {
		const c = bootApp();
		cms(c).save('page', null, { title: 'About' }, null);
		const second = cms(c).save('page', null, { title: 'About' }, null);
		expect(cms(c).get('page', second.id).slug).toBe('about-2');
	});

	test('update keeps the id, records a revision, and honours an explicit slug', () => {
		const c = bootApp();
		const created = cms(c).save('post', null, { title: 'First', tags: 'a, b' }, null);
		const updated = cms(c).save('post', created.id, { title: 'First edited', slug: 'custom', tags: '' }, null);
		expect(updated.id).toBe(created.id);
		const entry = cms(c).get('post', created.id);
		expect(entry.slug).toBe('custom');
		expect(entry.data.tags).toEqual([]);
		expect(cms(c).revisions(created.id)).toHaveLength(2);
	});

	test('tags are split from a comma-separated string', () => {
		const c = bootApp();
		const r = cms(c).save('post', null, { title: 'T', tags: ' svelte ,norns,, ' }, null);
		expect(cms(c).get('post', r.id).data.tags).toEqual(['svelte', 'norns']);
	});

	test('list() filters by status and searches titles', () => {
		const c = bootApp();
		const a = cms(c).save('post', null, { title: 'Alpha' }, null);
		cms(c).save('post', null, { title: 'Beta' }, null);
		cms(c).setStatus('post', a.id, 'published');
		expect(cms(c).list('post', { status: 'published' }).map((e) => e.title)).toEqual(['Alpha']);
		expect(cms(c).list('post', { q: 'bet' }).map((e) => e.title)).toEqual(['Beta']);
		expect(cms(c).count('post')).toBe(2);
		expect(cms(c).count('post', 'published')).toBe(1);
	});
});

describe('public resolution', () => {
	test('a published page resolves at /<slug> with rendered markdown', () => {
		const c = bootApp();
		const r = cms(c).save('page', null, { title: 'About us', body: '## Team\n\nHello **there**.' }, null);
		expect(cms(c).resolve('/about-us')).toBeNull();
		cms(c).setStatus('page', r.id, 'published');
		const res = cms(c).resolve('/about-us');
		expect(res.kind).toBe('entry');
		expect(res.entry.title).toBe('About us');
		expect(res.html.body).toContain('<h2>Team</h2>');
		expect(res.html.body).toContain('<strong>there</strong>');
	});

	test('drafts resolve only in preview mode', () => {
		const c = bootApp();
		cms(c).save('page', null, { title: 'Secret' }, null);
		expect(cms(c).resolve('/secret')).toBeNull();
		expect(cms(c).resolve('/secret', true).entry.title).toBe('Secret');
	});

	test('posts live under /posts and /posts lists the published ones', () => {
		const c = bootApp();
		const r = cms(c).save('post', null, { title: 'Launch' }, null);
		cms(c).save('post', null, { title: 'Draft only' }, null);
		cms(c).setStatus('post', r.id, 'published');
		expect(cms(c).resolve('/launch')).toBeNull();
		expect(cms(c).resolve('/posts/launch').entry.title).toBe('Launch');
		const list = cms(c).resolve('/posts');
		expect(list.kind).toBe('list');
		expect(list.entries.map((e) => e.title)).toEqual(['Launch']);
	});

	test('site nav shows listable types and pages flagged showInNav', () => {
		const c = bootApp();
		const a = cms(c).save('page', null, { title: 'About', showInNav: 'on' }, null);
		const b = cms(c).save('page', null, { title: 'Hidden' }, null);
		cms(c).setStatus('page', a.id, 'published');
		cms(c).setStatus('page', b.id, 'published');
		const nav = cms(c).nav();
		expect(nav).toEqual([{ label: 'About', href: '/about' }, { label: 'Posts', href: '/posts' }]);
	});
});

describe('auth', () => {
	test('login issues a session that resolves to the user, logout revokes it', () => {
		const c = bootApp();
		auth(c).createUser('Admin@Example.com', 'Ada', 'correct horse', 'admin');
		expect(auth(c).hasUsers()).toBe(true);
		expect(auth(c).login('admin@example.com', 'wrong')).toBeNull();
		const session = auth(c).login('admin@example.com', 'correct horse');
		expect(session.user.email).toBe('admin@example.com');
		expect(auth(c).userForSession(session.id).role).toBe('admin');
		auth(c).logout(session.id);
		expect(auth(c).userForSession(session.id)).toBeUndefined();
	});

	test('can(): admins do everything, editors cannot manage users', () => {
		const c = bootApp();
		expect(auth(c).can(null, 'edit')).toBe(false);
		expect(auth(c).can({ role: 'admin' }, 'manage-users')).toBe(true);
		expect(auth(c).can({ role: 'editor' }, 'manage-users')).toBe(false);
		expect(auth(c).can({ role: 'editor' }, 'edit')).toBe(true);
	});
});
