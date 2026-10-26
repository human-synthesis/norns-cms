import { describe, test, expect } from 'bun:test';
import { bootApp } from './helpers.js';
import { cms } from '../src/lib/norns/content/server/public.c';
import { seo, seoChecks } from '../src/lib/norns/seo/server/public.c';
import { settings } from '../src/lib/norns/settings/server/public.c';
import { auth } from '../src/lib/norns/auth/server/public.c';

const publish = async (c, type, raw, user = null) => {
	const r = await cms(c).save(type, null, raw, user);
	if (!r.ok) throw new Error(JSON.stringify(r.issues));
	await cms(c).setStatus(type, r.id, 'published');
	return cms(c).get(type, r.id);
};
const ld = (head) => JSON.parse(head.jsonLd)['@graph'];

describe('head and structured data', () => {
	test('an article gets title template, description, canonical, BlogPosting and breadcrumbs', async () => {
		const c = await bootApp();
		const id = await auth(c).createUser('w@example.com', 'Wren Writer', 'correct horse battery', 'owner');
		const user = await auth(c).getUser(id);
		await publish(c, 'article', { title: 'Shipping a CMS', summary: 'How we built it in a week.', body: '## Plan', tags: 'cms', keyFacts: 'One\nTwo', sources: 'Spec | https://example.com/spec' }, user);
		const res = await cms(c).resolve('/articles/shipping-a-cms');
		const head = await seo(c).forEntry(res, { authors: [{ name: 'Wren Writer', url: '/authors/wren-writer' }], sources: [{ title: 'Spec', url: 'https://example.com/spec' }] });
		expect(head.title).toBe('Shipping a CMS · Norns CMS');
		expect(head.description).toBe('How we built it in a week.');
		expect(head.canonical).toBe('http://localhost:5173/articles/shipping-a-cms');
		expect(head.robots).toContain('index');
		expect(head.markdown).toBe('http://localhost:5173/articles/shipping-a-cms.md');
		const graph = ld(head);
		const post = graph.find((g) => g['@type'] === 'BlogPosting');
		expect(post.author[0].name).toBe('Wren Writer');
		expect(post.abstract).toBe('One Two');
		expect(post.citation[0].url).toBe('https://example.com/spec');
		expect(graph.find((g) => g['@type'] === 'BreadcrumbList').itemListElement.map((i) => i.name)).toEqual(['Norns CMS', 'Articles', 'Shipping a CMS']);
	});

	test('a page with an FAQ block publishes FAQPage; noindex is honoured', async () => {
		const c = await bootApp();
		const blocks = JSON.stringify([{ type: 'faq', props: { items: [{ question: 'Is it free?', answer: 'Yes, **MIT**.' }] } }]);
		await publish(c, 'page', { title: 'Help', layout: 'blocks', blocks, seoNoindex: 'on' });
		const head = await seo(c).forEntry(await cms(c).resolve('/help'));
		const faq = ld(head).find((g) => g['@type'] === 'FAQPage');
		expect(faq.mainEntity[0].acceptedAnswer.text).toBe('Yes, MIT.');
		expect(head.robots).toContain('noindex');
		expect(head.markdown).toBeNull();
	});

	test('JSON-LD cannot break out of its script tag', async () => {
		const c = await bootApp();
		await publish(c, 'page', { title: '</script><script>alert(1)</script>', layout: 'markdown' });
		const [e] = await cms(c).list('page');
		const head = await seo(c).forEntry(await cms(c).resolve(`/${e.slug}`));
		expect(head.jsonLd).not.toContain('</script>');
	});
});

describe('crawl surface', () => {
	test('sitemap lists live, indexable entries only', async () => {
		const c = await bootApp();
		await publish(c, 'article', { title: 'Visible' });
		await publish(c, 'article', { title: 'Hidden', seoNoindex: 'on' });
		await cms(c).save('article', null, { title: 'Draft' }, null);
		await publish(c, 'article', { title: 'Members', visibility: 'members' });
		const xml = await seo(c).sitemapFor('article');
		expect(xml).toContain('/articles/visible</loc>');
		expect(xml).not.toContain('hidden');
		expect(xml).not.toContain('draft');
		expect(xml).not.toContain('members');
		expect(await seo(c).sitemapIndex()).toContain('/sitemap-page.xml');
	});

	test('robots.txt blocks the AI crawlers switched off', async () => {
		const c = await bootApp();
		await settings(c).patch('seo', { allow_GPTBot: false, robotsExtra: 'Crawl-delay: 2' }, null);
		const txt = await seo(c).robots();
		expect(txt).toContain('User-agent: GPTBot\nDisallow: /');
		expect(txt).not.toContain('User-agent: ClaudeBot');
		expect(txt).toContain('Crawl-delay: 2');
		expect(txt).toContain('Sitemap: http://localhost:5173/sitemap.xml');
	});

	test('llms.txt and Markdown twins expose public content only', async () => {
		const c = await bootApp();
		await publish(c, 'article', { title: 'Open', summary: 'For everyone.', body: '## Part\n\nText' });
		await publish(c, 'article', { title: 'Closed', visibility: 'members' });
		await publish(c, 'article', { title: 'Opted out', seoNoLlms: 'on' });
		const txt = await seo(c).llms();
		expect(txt).toContain('- [Open](http://localhost:5173/articles/open.md): For everyone.');
		expect(txt).not.toContain('Closed');
		expect(txt).not.toContain('Opted out');
		const md = await seo(c).twin('/articles/open');
		expect(md).toContain('title: "Open"');
		expect(md).toContain('## Part');
		expect(await seo(c).twin('/articles/closed')).toBeNull();
	});

	test('RSS lists published articles with full content', async () => {
		const c = await bootApp();
		await publish(c, 'article', { title: 'Feed me', body: 'Hello **feed**' });
		const xml = await seo(c).feed();
		expect(xml).toContain('<title>Feed me</title>');
		expect(xml).toContain('<content:encoded><![CDATA[<p>Hello <strong>feed</strong></p>');
	});
});

describe('redirects', () => {
	test('renaming a published slug adds a 301 and flattens chains', async () => {
		const c = await bootApp();
		const e = await publish(c, 'article', { title: 'One' });
		await cms(c).save('article', e.id, { title: 'One', slug: 'two' }, null);
		await cms(c).save('article', e.id, { title: 'One', slug: 'three' }, null);
		const list = await seo(c).redirects();
		const byFrom = Object.fromEntries(list.map((r) => [r.from_path, r.to_path]));
		expect(byFrom['/articles/one']).toBe('/articles/three');
		expect(byFrom['/articles/two']).toBe('/articles/three');
		expect(byFrom['/posts']).toBe('/articles');
	});

	test('manual redirects refuse loops and self-references; import reports errors', async () => {
		const c = await bootApp();
		expect((await seo(c).saveRedirect(null, { from: '/a', to: '/b', status: 301 })).ok).toBe(true);
		expect((await seo(c).saveRedirect(null, { from: '/b', to: '/a', status: 301 })).ok).toBe(false);
		expect((await seo(c).saveRedirect(null, { from: '/c', to: '/c/', status: 301 })).ok).toBe(false);
		const r = await seo(c).importRedirects('/old,/new\n# comment\n/gone,,410\n/x,/x');
		expect(r.imported).toBe(2);
		expect(r.errors).toHaveLength(1);
	});
});

describe('checks', () => {
	test('editor checks flag missing descriptions, long titles, h1s and missing alt text', () => {
		const issues = seoChecks({ title: 'A'.repeat(70), body: '# Big\n\n![](media:1)' }, { siteTitle: 'Site', template: '%s · %site' }).map((i) => i.message);
		expect(issues.some((m) => m.includes('characters; search engines cut it'))).toBe(true);
		expect(issues.some((m) => m.includes('Add a summary'))).toBe(true);
		expect(issues.some((m) => m.includes('level-1 heading'))).toBe(true);
		expect(issues.some((m) => m.includes('no alt text'))).toBe(true);
	});
});
