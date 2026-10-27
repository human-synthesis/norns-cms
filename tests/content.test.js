import { describe, test, expect } from 'bun:test';
import { bootApp, modules } from './helpers.js';
import { cms } from '../src/lib/norns/content/server/public.c';
import { auth } from '../src/lib/norns/auth/server/public.c';
import { articles } from '../src/lib/norns/articles/server/public.c';
import { media } from '../src/lib/norns/media/server/public.c';
import { settings } from '../src/lib/norns/settings/server/public.c';

const owner = async (c) => {
	const id = await auth(c).createUser('owner@example.com', 'Olga Owner', 'correct horse battery', 'owner');
	return { ...(await auth(c).getUser(id)), capabilities: ['*'] };
};

describe('content types', () => {
	test('pages and articles are registered regardless of module order', async () => {
		const c = await bootApp([...modules.map(([, m]) => m)].reverse());
		expect(cms(c).types().map((t) => t.id)).toEqual(['page', 'article']);
	});

	test('the SEO field group adds a tab and fields to every type', async () => {
		const form = await cms(await bootApp()).form('article');
		expect(form.tabs.map((t) => t.id)).toEqual(['content', 'details', 'seo']);
		expect(form.fields.find((f) => f.name === 'seoTitle').tab).toBe('seo');
		expect(form.blocks.map((b) => b.id)).toContain('articleList');
		expect(form.blocks.find((b) => b.id === 'articleList').resolve).toBeUndefined();
	});

	test('admin nav is a tree filtered by capability', async () => {
		const c = await bootApp();
		const hrefs = (nav) => nav.flatMap((g) => g.items.flatMap((i) => [i.href, ...i.children.map((ch) => ch.href)])).filter(Boolean);
		const full = cms(c).adminNav({ status: 'active', capabilities: ['*'] });
		expect(full.map((g) => g.section)).toEqual(['Content', 'People', 'Marketing', 'System']);
		const articles = full[0].items.find((i) => i.id === 'type-article');
		expect(articles.children.map((ch) => ch.label)).toEqual(['All articles', 'Waiting for review', 'New article', 'Categories', 'Tags']);
		expect(hrefs(full)).toContain('/admin/users?kind=client');
		expect(hrefs(full)).toContain('/admin/consent/scripts');
		const author = cms(c).adminNav({ status: 'active', capabilities: ['admin.access', 'content.view', 'content.create', 'media.view'] });
		expect(hrefs(author)).toContain('/admin/content/article/new');
		expect(hrefs(author)).toContain('/admin/media');
		expect(hrefs(author)).not.toContain('/admin/users?kind=staff');
		expect(hrefs(author)).not.toContain('/admin/taxonomy/tag');
		// A parent whose children are all hidden disappears (Logs needs audit.view or settings.manage).
		expect(author.flatMap((g) => g.items.map((i) => i.id))).not.toContain('logs');
		expect(author.flatMap((g) => g.items.map((i) => i.id))).not.toContain('settings');
	});
});

describe('entries', () => {
	test('save() rejects a missing title with a field-level issue', async () => {
		const r = await cms(await bootApp()).save('page', null, { body: 'x' }, null);
		expect(r.ok).toBe(false);
		expect(r.issues[0].path[0].key).toBe('title');
		expect(r.issues[0].message).toBe('Title is required');
	});

	test('save() creates a draft with a derived slug and cached HTML', async () => {
		const c = await bootApp();
		const r = await cms(c).save('page', null, { title: 'Hello, World!', layout: 'markdown', body: '## Hi\n\n:::tip Note\nInside\n:::' }, null);
		expect(r.ok).toBe(true);
		const e = await cms(c).get('page', r.id);
		expect(e.slug).toBe('hello-world');
		expect(e.status).toBe('draft');
		expect(e.rendered.body.html).toContain('<h2 id="hi">');
		expect(e.rendered.body.html).toContain('callout-tip');
	});

	test('duplicate slugs get a numeric suffix; explicit slugs are honoured', async () => {
		const c = await bootApp();
		await cms(c).save('page', null, { title: 'About', layout: 'markdown' }, null);
		const second = await cms(c).save('page', null, { title: 'About', layout: 'markdown' }, null);
		expect((await cms(c).get('page', second.id)).slug).toBe('about-2');
		const third = await cms(c).save('page', null, { title: 'Team', layout: 'markdown', slug: 'about/team' }, null);
		expect((await cms(c).get('page', third.id)).slug).toBe('about/team');
	});

	test('revisions are kept and restore goes through validation', async () => {
		const c = await bootApp();
		const u = await owner(c);
		const a = await cms(c).save('article', null, { title: 'First', body: 'one' }, u);
		await cms(c).save('article', a.id, { title: 'Second', body: 'two' }, u);
		const revs = await cms(c).revisions(a.id);
		expect(revs).toHaveLength(2);
		const oldest = revs[revs.length - 1];
		const restored = await cms(c).restore('article', a.id, oldest.id, u);
		expect(restored.ok).toBe(true);
		const e = await cms(c).get('article', a.id);
		expect(e.title).toBe('First');
		expect(e.data.body).toBe('one');
		expect(await cms(c).revisions(a.id)).toHaveLength(3);
	});

	test('blocks are validated per block type and rendered', async () => {
		const c = await bootApp();
		const bad = await cms(c).save('page', null, { title: 'B', layout: 'blocks', blocks: JSON.stringify([{ type: 'nope', props: {} }]) }, null);
		expect(bad.ok).toBe(false);
		const missing = await cms(c).save('page', null, { title: 'B', layout: 'blocks', blocks: JSON.stringify([{ type: 'hero', props: {} }]) }, null);
		expect(missing.ok).toBe(false);
		expect(missing.issues[0].message).toContain('Heading is required');
		const blocks = [
			{ type: 'hero', props: { title: 'Welcome', text: 'Hi' } },
			{ type: 'faq', props: { items: [{ question: 'Why?', answer: 'Because **yes**' }] } }
		];
		const ok = await cms(c).save('page', null, { title: 'Home', layout: 'blocks', blocks: JSON.stringify(blocks) }, null);
		expect(ok.ok).toBe(true);
		const e = await cms(c).get('page', ok.id);
		expect(e.data.blocks).toHaveLength(2);
		expect(e.data.blocks[0].id).toBeTruthy();
		expect(e.data.blocks[1].props.title).toBe('Frequently asked questions');
		expect(e.rendered.blocks[1].items[0].answer.html).toContain('<strong>yes</strong>');
	});
});

describe('publishing', () => {
	test('scheduled entries stay hidden until their time; drafts only in preview', async () => {
		const c = await bootApp();
		const a = await cms(c).save('article', null, { title: 'Later' }, null);
		await cms(c).setStatus('article', a.id, 'published', { publishAt: Date.now() + 3600_000 });
		expect(await cms(c).resolve('/articles/later')).toBeNull();
		expect((await cms(c).resolve('/articles/later', { preview: true })).entry.title).toBe('Later');
		expect(await cms(c).count('article', { status: 'scheduled' })).toBe(1);
		await cms(c).setStatus('article', a.id, 'published', { publishAt: Date.now() - 1000 });
		expect((await cms(c).resolve('/articles/later')).entry.title).toBe('Later');
	});

	test('members-only entries resolve locked without their body for the public', async () => {
		const c = await bootApp();
		const a = await cms(c).save('article', null, { title: 'Secret', summary: 'Teaser', body: 'The **body**', visibility: 'members' }, null);
		await cms(c).setStatus('article', a.id, 'published');
		const pub = await cms(c).resolve('/articles/secret');
		expect(pub.locked).toBe(true);
		expect(pub.entry.data.body).toBeUndefined();
		expect(pub.entry.data.summary).toBe('Teaser');
		expect(pub.html.body).toBeUndefined();
		const member = await cms(c).resolve('/articles/secret', { member: true });
		expect(member.locked).toBe(false);
		expect(member.html.body).toContain('<strong>body</strong>');
		expect((await cms(c).query('article')).entries).toHaveLength(0);
		expect((await cms(c).query('article', { member: true })).entries).toHaveLength(1);
	});

	test('site nav shows pages flagged showInNav in navOrder, then listable types', async () => {
		const c = await bootApp();
		const a = await cms(c).save('page', null, { title: 'About', layout: 'markdown', showInNav: 'on', navOrder: '2' }, null);
		const b = await cms(c).save('page', null, { title: 'Contact', layout: 'markdown', showInNav: 'on', navOrder: '1', navLabel: 'Say hi' }, null);
		const h = await cms(c).save('page', null, { title: 'Hidden', layout: 'markdown' }, null);
		for (const r of [a, b, h]) await cms(c).setStatus('page', r.id, 'published');
		expect(await cms(c).nav()).toEqual([
			{ label: 'Say hi', href: '/contact' },
			{ label: 'About', href: '/about' },
			{ label: 'Articles', href: '/articles', typeId: 'article' }
		]);
	});
});

describe('articles', () => {
	test('computed fields, default author, tags and category become terms', async () => {
		const c = await bootApp();
		const u = await owner(c);
		const cat = await articles(c).saveTerm('category', null, { name: 'Guides' });
		const r = await cms(c).save('article', null, { title: 'How to', body: 'word '.repeat(450), tags: 'Svelte, CMS', category: String(cat.id) }, u);
		const e = await cms(c).get('article', r.id);
		expect(e.data.readingTime).toBe(2);
		expect(e.data.authors).toEqual([u.id]);
		expect((await articles(c).terms('tag')).map((t) => t.slug).sort()).toEqual(['cms', 'svelte']);
		await cms(c).setStatus('article', r.id, 'published');
		const [d] = (await articles(c).list({ termId: cat.id })).entries;
		expect(d.category.name).toBe('Guides');
		expect(d.authors[0].name).toBe('Olga Owner');
		expect(d.authors[0].url).toBe('/authors/olga-owner');
		expect(d.tagTerms.map((t) => t.name).sort()).toEqual(['CMS', 'Svelte']);
	});

	test('merging tags rewrites entries', async () => {
		const c = await bootApp();
		const u = await owner(c);
		const r = await cms(c).save('article', null, { title: 'T', tags: 'js' }, u);
		await cms(c).save('article', null, { title: 'U', tags: 'JavaScript' }, u);
		const tags = await articles(c).terms('tag');
		const js = tags.find((t) => t.slug === 'js');
		const full = tags.find((t) => t.slug === 'javascript');
		expect((await articles(c).mergeTerm(js.id, full.id, u)).ok).toBe(true);
		expect((await cms(c).get('article', r.id)).data.tags).toEqual(['JavaScript']);
		expect((await articles(c).terms('tag')).map((t) => t.slug)).toEqual(['javascript']);
	});

	test('submitting for review emails editors (log provider)', async () => {
		const c = await bootApp();
		const u = await owner(c);
		await auth(c).createUser('editor@example.com', 'Ed', 'correct horse battery', 'editor');
		const authorId = await auth(c).createUser('author@example.com', 'Au', 'correct horse battery', 'author');
		const author = await auth(c).getUser(authorId);
		const r = await cms(c).save('article', null, { title: 'Draft for review' }, author);
		await cms(c).setStatus('article', r.id, 'in_review', { user: author });
		const { mail } = await import('../src/lib/norns/mail/server/public.c');
		expect((await mail(c).lastTo('editor@example.com')).subject).toBe('Review requested: Draft for review');
		expect((await mail(c).lastTo(u.email)).subject).toBe('Review requested: Draft for review');
	});
});

describe('media', () => {
	const png = () => {
		const b = new Uint8Array(33);
		b.set([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0, 0, 0, 13, 0x49, 0x48, 0x44, 0x52]);
		b.set([0, 0, 2, 0, 0, 0, 1, 0], 16); // 512 x 256
		return new File([b], 'My Photo.PNG', { type: 'image/png' });
	};

	test('uploads sniff the type, read dimensions and refuse disguised files', async () => {
		const c = await bootApp();
		const up = await media(c).upload(png(), { alt: 'A photo' });
		expect(up.ok).toBe(true);
		expect(up.media.width).toBe(512);
		expect(up.media.height).toBe(256);
		expect(up.media.url).toMatch(/^\/media\/\d{4}\/\d{2}\/[a-f0-9]+-My-Photo\.png$/);
		const fake = await media(c).upload(new File(['<svg onload=alert(1)>'], 'x.png', { type: 'image/png' }));
		expect(fake.ok).toBe(false);
	});

	test('usage is tracked from image fields and markdown, and guards deletion', async () => {
		const c = await bootApp();
		const up = await media(c).upload(png(), { alt: 'Alt text' });
		const r = await cms(c).save('article', null, { title: 'Pic', body: `Look ![](media:${up.media.id} "Caption")` }, null);
		const e = await cms(c).get('article', r.id);
		expect(e.rendered.body.html).toContain('alt="Alt text"');
		expect(e.rendered.body.html).toContain('<figcaption>Caption</figcaption>');
		expect((await media(c).usage(up.media.id)).map((u) => u.id)).toEqual([r.id]);
		expect((await media(c).remove(up.media.id)).ok).toBe(false);
		await cms(c).save('article', r.id, { title: 'Pic', body: 'no image' }, null);
		expect((await media(c).remove(up.media.id)).ok).toBe(true);
	});

	test('image resizing setting produces srcsets', async () => {
		const c = await bootApp();
		await settings(c).patch('site', { imageResizing: true }, null);
		const up = await media(c).upload(png());
		expect(up.media.srcset).toContain('/cdn-cgi/image/width=480');
	});
});

describe('settings', () => {
	test('defaults, validation and saving', async () => {
		const c = await bootApp();
		expect((await settings(c).get('site')).registration).toBe('off');
		const bad = await settings(c).save('site', { title: '', registration: 'off' }, null);
		expect(bad.ok).toBe(false);
		const ok = await settings(c).save('site', { title: 'My site', registration: 'open' }, null);
		expect(ok.ok).toBe(true);
		expect((await settings(c).get('site')).title).toBe('My site');
		expect((await settings(c).get('site')).registration).toBe('open');
	});
});

describe('renamed types', () => {
	test('the old post id resolves to articles by alias', async () => {
		const c = await bootApp();
		expect(cms(c).getType('post')).toBeUndefined();
		expect(cms(c).typeByAlias('post').id).toBe('article');
	});
});
