import { describe, test, expect } from 'bun:test';
import { bootApp } from './helpers.js';
import { cms } from '../src/lib/norns/content/server/public.c';
import { comments, commentHtml } from '../src/lib/norns/comments/server/public.c';
import { settings } from '../src/lib/norns/settings/server/public.c';
import { auth } from '../src/lib/norns/auth/server/public.c';
import { seo } from '../src/lib/norns/seo/server/public.c';

const publish = async (c, type, raw) => {
	const r = await cms(c).save(type, null, raw, null);
	if (!r.ok) throw new Error(JSON.stringify(r.issues));
	await cms(c).setStatus(type, r.id, 'published');
	return cms(c).get(type, r.id);
};
const configure = async (c, values) => {
	const r = await settings(c).save('comments', { ...(await settings(c).get('comments')), ...values }, null);
	if (!r.ok) throw new Error(JSON.stringify(r.issues));
};
async function people(c) {
	const memberId = await auth(c).createUser('m@example.com', 'Mia Member', 'correct horse battery', 'member');
	const staffId = await auth(c).createUser('s@example.com', 'Sam Staff', 'correct horse battery', 'editor');
	// Sessions carry the role's capabilities; getUser alone does not.
	return {
		member: { ...(await auth(c).getUser(memberId)), capabilities: ['account.access', 'content.members'] },
		staff: { ...(await auth(c).getUser(staffId)), capabilities: ['admin.access', 'content.*', 'comments.moderate'] }
	};
}
const guest = { name: 'Gus Guest', email: 'gus@example.com' };

describe('where comments are on', () => {
	test('articles by default, pages when ticked, and each entry can open, close or hide them', async () => {
		const c = await bootApp();
		const article = await publish(c, 'article', { title: 'News' });
		const page = await publish(c, 'page', { title: 'About', layout: 'markdown', body: 'x' });
		const [at, pt] = [cms(c).getType('article'), cms(c).getType('page')];
		expect(await comments(c).modeFor(at, article)).toBe('open');
		expect(await comments(c).modeFor(pt, page)).toBe('off');
		expect((await cms(c).form('page')).fields.find((f) => f.name === 'commentsMode')).toBeTruthy();

		await configure(c, { types: ['article', 'page'] });
		expect(await comments(c).modeFor(pt, page)).toBe('open');
		await configure(c, { types: [] });
		const opened = await cms(c).save('page', page.id, { title: 'About', layout: 'markdown', body: 'x', commentsMode: 'open' }, null);
		expect(await comments(c).modeFor(pt, opened.entry)).toBe('open');
		const closed = await cms(c).save('article', article.id, { title: 'News', commentsMode: 'closed' }, null);
		expect(await comments(c).modeFor(at, closed.entry)).toBe('closed');
		const off = await cms(c).save('article', article.id, { title: 'News', commentsMode: 'off' }, null);
		expect(await comments(c).modeFor(at, off.entry)).toBe('off');

		await configure(c, { enabled: false });
		expect(await comments(c).modeFor(pt, opened.entry)).toBe('off');
	});

	test('the types setting only keeps real content types; old entries close after the set days', async () => {
		const c = await bootApp();
		await configure(c, { types: ['article', 'nope'], closeAfterDays: 1 });
		expect((await comments(c).config()).types).toEqual(['article']);
		const e = await publish(c, 'article', { title: 'Old' });
		const at = cms(c).getType('article');
		expect(await comments(c).modeFor(at, e)).toBe('open');
		expect(await comments(c).modeFor(at, { ...e, published_at: Date.now() - 2 * 86400000 })).toBe('closed');
	});
});

describe('writing and replying', () => {
	test('members only by default: visitors are told to sign in; a member comment appears at once', async () => {
		const c = await bootApp();
		const { member } = await people(c);
		const e = await publish(c, 'article', { title: 'Hello' });
		const at = cms(c).getType('article');
		const asGuest = await comments(c).section(at, e, {});
		expect(asGuest).toMatchObject({ mode: 'open', canPost: false, signIn: true, count: 0 });
		expect((await comments(c).post({ entryId: e.id, body: 'Hi', ...guest })).status).toBe(403);

		const r = await comments(c).post({ entryId: e.id, body: 'First!\n\nSee https://example.com <b>bold</b>' }, { user: member });
		expect(r).toMatchObject({ ok: true, status: 'approved' });
		const s = await comments(c).section(at, e, { user: member });
		expect(s).toMatchObject({ count: 1, canPost: true, signIn: false, guest: false });
		expect(s.comments[0]).toMatchObject({ name: 'Mia Member', depth: 0, pending: false, canReply: true });
		expect(s.comments[0].html).toBe('<p>First!</p><p>See <a href="https://example.com" rel="nofollow ugc noopener" target="_blank">https://example.com</a> &lt;b&gt;bold&lt;/b&gt;</p>');
	});

	test('visitors when allowed: name and email needed, held for approval, seen only by moderators', async () => {
		const c = await bootApp();
		await configure(c, { who: 'anyone' });
		const e = await publish(c, 'article', { title: 'Open thread' });
		const at = cms(c).getType('article');
		const bad = await comments(c).post({ entryId: e.id, body: 'x', email: 'nope' });
		expect(Object.keys(bad.errors).sort()).toEqual(['body', 'email', 'name']);
		const r = await comments(c).post({ entryId: e.id, body: 'Nice post', ...guest });
		expect(r.status).toBe('pending');
		expect((await comments(c).section(at, e, {})).comments).toHaveLength(0);
		expect((await comments(c).list({ status: 'pending' })).comments[0]).toMatchObject({ author_email: 'gus@example.com', entry: { title: 'Open thread' } });
		await comments(c).setStatus(r.id, 'approved');
		expect((await comments(c).section(at, e, {})).comments[0].name).toBe('Gus Guest');
	});

	test('replies nest up to the set depth; a deleted comment keeps its replies', async () => {
		const c = await bootApp();
		await configure(c, { maxDepth: 2 });
		const { member } = await people(c);
		const e = await publish(c, 'article', { title: 'Thread' });
		const at = cms(c).getType('article');
		const a = await comments(c).post({ entryId: e.id, body: 'Root' }, { user: member });
		const b = await comments(c).post({ entryId: e.id, parentId: a.id, body: 'Level one' }, { user: member });
		const d = await comments(c).post({ entryId: e.id, parentId: b.id, body: 'Level two' }, { user: member });
		expect((await comments(c).post({ entryId: e.id, parentId: d.id, body: 'Too deep' }, { user: member })).errors.body).toContain('cannot reply');
		await comments(c).post({ entryId: e.id, body: 'Second root' }, { user: member });
		let s = await comments(c).section(at, e, {});
		expect(s.comments.map((x) => [x.html, x.depth, x.canReply])).toEqual([
			['<p>Root</p>', 0, true],
			['<p>Level one</p>', 1, true],
			['<p>Level two</p>', 2, false],
			['<p>Second root</p>', 0, true]
		]);
		await comments(c).remove(b.id);
		s = await comments(c).section(at, e, {});
		expect(s.comments.find((x) => x.html === '<p>Level two</p>').parentId).toBe(a.id);
	});

	test('closed sections take no comments (the team may still answer); members-only entries need access', async () => {
		const c = await bootApp();
		const { member, staff } = await people(c);
		const e = await publish(c, 'article', { title: 'Closed', commentsMode: 'closed' });
		expect((await comments(c).post({ entryId: e.id, body: 'Late' }, { user: member })).status).toBe(403);
		const team = await comments(c).post({ entryId: e.id, body: 'Update from us' }, { user: staff });
		expect(team).toMatchObject({ ok: true, status: 'approved' });
		expect((await comments(c).get(team.id)).staff).toBe(1);

		await configure(c, { who: 'anyone' });
		const locked = await publish(c, 'article', { title: 'Members', visibility: 'members' });
		expect((await comments(c).post({ entryId: locked.id, body: 'Hi', ...guest })).status).toBe(404);
		const draft = await cms(c).save('article', null, { title: 'Draft' }, null);
		expect((await comments(c).post({ entryId: draft.id, body: 'Hi', ...guest })).status).toBe(404);
	});

	test('spam: too many links or a failed protection check; moderation "all" holds members too', async () => {
		const c = await bootApp();
		await configure(c, { moderation: 'all', maxLinks: 1 });
		const { member } = await people(c);
		const e = await publish(c, 'article', { title: 'Spam target' });
		expect((await comments(c).post({ entryId: e.id, body: 'a https://x.io b https://y.io' }, { user: member })).status).toBe('spam');
		expect((await comments(c).post({ entryId: e.id, body: 'Looks fine' }, { user: member, spam: true })).status).toBe('spam');
		expect((await comments(c).post({ entryId: e.id, body: 'Looks fine' }, { user: member })).status).toBe('pending');
		expect(await comments(c).counts()).toEqual({ spam: 2, pending: 1 });
	});
});

describe('moderation and the rest of the CMS', () => {
	test('the team answers from the admin; edits; structured data lists approved comments', async () => {
		const c = await bootApp();
		await configure(c, { who: 'anyone' });
		const { staff } = await people(c);
		const e = await publish(c, 'article', { title: 'Q and A' });
		const q = await comments(c).post({ entryId: e.id, body: 'How?', ...guest });
		const a = await comments(c).reply(q.id, 'Like this.', staff);
		expect(a.ok).toBe(true);
		expect((await comments(c).get(q.id)).status).toBe('approved'); // answering approves
		expect((await comments(c).get(a.id)).parent_id).toBe(q.id);
		await comments(c).edit(q.id, 'How does it work?');
		const graph = JSON.parse((await seo(c).forEntry(await cms(c).resolve('/articles/q-and-a'), { comments: await comments(c).approved(e.id) })).jsonLd)['@graph'];
		const post = graph.find((g) => g['@type'] === 'BlogPosting');
		expect(post.commentCount).toBe(2);
		expect(post.comment[0]).toMatchObject({ '@type': 'Comment', text: 'How does it work?', author: { name: 'Gus Guest' } });
		expect(post.comment[1].author['@id']).toContain('#organization');
	});

	test('deleting an entry deletes its comments; deleted members are anonymised by the daily purge', async () => {
		const c = await bootApp();
		const { member } = await people(c);
		const e = await publish(c, 'article', { title: 'Gone soon' });
		const keep = await publish(c, 'article', { title: 'Stays' });
		await comments(c).post({ entryId: e.id, body: 'Bye' }, { user: member });
		const mine = await comments(c).post({ entryId: keep.id, body: 'Mine' }, { user: member });
		await cms(c).remove('article', e.id);
		expect((await comments(c).list({})).total).toBe(1);
		expect(await comments(c).byUser(member.id)).toHaveLength(1);
		await c.resolve('auth.repo').update(member.id, { status: 'deleted' });
		expect(await comments(c).purge()).toMatchObject({ anonymized: 1 });
		expect(await comments(c).get(mine.id)).toMatchObject({ author_name: 'Former member', author_email: '', user_id: null });
	});

	test('comment text is escaped; only http(s) links become links', () => {
		expect(commentHtml('<script>x</script>\njavascript:alert(1)')).toBe('<p>&lt;script&gt;x&lt;/script&gt;<br>javascript:alert(1)</p>');
		expect(commentHtml('"https://a.io/?q=1&b=2"')).toContain('href="https://a.io/?q=1&amp;b=2"');
	});
});
