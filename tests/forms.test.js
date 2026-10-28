import { describe, test, expect, afterEach } from 'bun:test';
import { bootApp } from './helpers.js';
import { forms } from '../src/lib/norns/forms/server/public.c';
import { settings } from '../src/lib/norns/settings/server/public.c';
import { mail } from '../src/lib/norns/mail/server/public.c';
import { i18n } from '../src/lib/norns/i18n/server/public.c';
import { cms } from '../src/lib/norns/content/server/public.c';
import { auth } from '../src/lib/norns/auth/server/public.c';
import { apiKeys, handleApi } from '../src/lib/norns/api/server/public.c';

const human = () => ({ _t: String(Date.now() - 5000), _hp: '' });
let ipN = 0;
const ctx = (o = {}) => ({ lang: 'en', page: '/contact', ip: `10.0.0.${++ipN}`, ...o });

async function setup() {
	const c = await bootApp();
	await settings(c).patch('mail', { notifyTo: ['team@example.com'] }, null);
	return c;
}

describe('default forms', () => {
	test('contact and newsletter exist, translated', async () => {
		const c = await setup();
		const list = await forms(c).list();
		expect(list.map((f) => f.slug)).toEqual(['contact', 'newsletter']);
		await i18n(c).saveLanguage({ code: 'de' });
		const de = await forms(c).localized('contact', 'de');
		expect(de.submitLabel).toBe('Nachricht senden');
		expect(de.fields.find((f) => f.name === 'message').label).toBe('Nachricht');
		const en = await forms(c).localized('newsletter', 'en');
		expect(en.fields.map((f) => f.type)).toEqual(['email', 'consent']);
	});

	test('the defaults can be switched off but not deleted', async () => {
		const c = await setup();
		const contact = await forms(c).bySlug('contact');
		expect((await forms(c).remove(contact.id)).ok).toBe(false);
		await forms(c).save(contact.id, { enabled: false });
		expect(await forms(c).localized('contact', 'en')).toBeNull();
		expect((await forms(c).submit('contact', {}, ctx())).status).toBe(404);
	});
});

describe('submitting', () => {
	test('validation errors come back per field, in the visitor’s language', async () => {
		const c = await setup();
		await i18n(c).saveLanguage({ code: 'de' });
		const r = await forms(c).submit('contact', { ...human(), name: '', email: 'nope', message: 'Hi' }, ctx({ lang: 'de' }));
		expect(r.ok).toBe(false);
		expect(r.errors).toEqual({ name: 'Name ist erforderlich', email: 'Geben Sie eine gültige E-Mail-Adresse ein', consent: 'Bitte setzen Sie hier ein Häkchen, um fortzufahren.' });
	});

	test('a valid message is stored and the team is notified, replies to the sender', async () => {
		const c = await setup();
		const r = await forms(c).submit('contact', { ...human(), name: 'Ada', email: 'Ada@Example.com', message: 'Hello there', consent: 'on' }, ctx());
		expect(r.ok).toBe(true);
		expect(r.message).toContain('Thanks');
		const s = await forms(c).submission(r.id);
		expect(s).toMatchObject({ email: 'ada@example.com', status: 'new', notified: 1, locale: 'en', page: '/contact' });
		expect(s.data.consent__text).toContain('I agree');
		const m = await mail(c).lastTo('team@example.com');
		expect(m.subject).toBe('New message from Ada — Norns CMS');
		expect(m.body).toContain('Message: Hello there');
	});

	test('spam: a protection verdict or too many links — stored as spam, nobody notified', async () => {
		const c = await setup();
		const base = { name: 'X', email: 'x@example.com', message: 'Hi', consent: 'on' };
		const a = await forms(c).submit('contact', base, ctx({ spam: true }));
		const b = await forms(c).submit('contact', { ...base, email: 'y@example.com' }, ctx({ spam: true }));
		const d = await forms(c).submit('contact', { ...human(), ...base, message: 'http://a http://b http://c http://d' }, ctx());
		for (const r of [a, b, d]) {
			expect(r.ok).toBe(true);
			expect((await forms(c).submission(r.id)).status).toBe('spam');
		}
		expect(await mail(c).lastTo('team@example.com')).toBeUndefined();
		expect(await forms(c).count({})).toBe(0);
		expect(await forms(c).count({ status: 'spam' })).toBe(3);
	});

	test('newsletter: one signup per email', async () => {
		const c = await setup();
		const first = await forms(c).submit('newsletter', { ...human(), email: 'n@example.com', consent: 'on' }, ctx());
		const again = await forms(c).submit('newsletter', { ...human(), email: 'N@example.com', consent: 'on' }, ctx());
		expect(first.duplicate).toBeUndefined();
		expect(again).toMatchObject({ ok: true, duplicate: true, message: 'You’re already subscribed.' });
		expect(await forms(c).count({ formId: first && (await forms(c).bySlug('newsletter')).id })).toBe(1);
	});

	test('ten submissions per IP and form in ten minutes', async () => {
		const c = await setup();
		const one = { lang: 'en', page: '/', ip: '9.9.9.9' };
		for (let i = 0; i < 10; i++) await forms(c).submit('newsletter', { ...human(), email: `r${i}@example.com`, consent: 'on' }, one);
		expect((await forms(c).submit('newsletter', { ...human(), email: 'r10@example.com', consent: 'on' }, one)).status).toBe(429);
	});
});

describe('editing and inbox', () => {
	test('fields, options and redirects are validated', async () => {
		const c = await setup();
		const save = (fields, settings = {}) => forms(c).save(null, { name: 'Quote', fields, settings });
		expect((await save([{ label: 'A', type: 'text' }, { label: 'A', type: 'email' }])).error).toContain('Two fields');
		expect((await save([{ label: 'Size', type: 'select' }])).error).toContain('option');
		expect((await save([{ label: 'E', type: 'email' }], { redirectTo: 'https://evil.example' })).field).toBe('redirectTo');
		const ok = await save([{ label: 'Your size', type: 'select', options: 'Small\nLarge', required: true }], { notifyTo: 'a@example.com, b@example.com' });
		expect(ok.ok).toBe(true);
		const f = await forms(c).get(ok.id);
		expect(f.fields[0]).toMatchObject({ name: 'your_size', options: [{ value: 'small', label: 'Small' }, { value: 'large', label: 'Large' }] });
		expect(f.settings.notifyTo).toEqual(['a@example.com', 'b@example.com']);
		expect((await forms(c).submit('quote', { ...human(), your_size: 'medium' }, ctx())).errors.your_size).toContain('not valid');
	});

	test('status changes and CSV export', async () => {
		const c = await setup();
		const r = await forms(c).submit('contact', { ...human(), name: 'Bo, "B"', email: 'bo@example.com', message: 'Line', consent: 'on' }, ctx());
		await forms(c).setStatus([r.id], 'archived');
		expect((await forms(c).submission(r.id)).status).toBe('archived');
		const { csv } = await forms(c).csv((await forms(c).bySlug('contact')).id);
		expect(csv.split('\n')[0]).toBe('submitted_at,status,language,page,name,email,message,consent');
		expect(csv).toContain('"Bo, ""B"""');
	});

	test('the form block resolves the form in the page’s language', async () => {
		const c = await setup();
		await i18n(c).saveLanguage({ code: 'de' });
		const block = cms(c).blocks().find((b) => b.id === 'form');
		const data = await block.resolve({ form: 'newsletter' }, { c, entry: { locale: 'de' } });
		expect(data.form.submitLabel).toBe('Abonnieren');
	});
});

describe('API', () => {
	test('forms and submissions over /api/v1', async () => {
		const c = await setup();
		const ownerId = await auth(c).createUser('o@example.com', 'O', 'correct horse battery', 'owner');
		const owner = { ...(await auth(c).getUser(ownerId)), capabilities: ['*'] };
		const { key } = await apiKeys(c).create({ name: 'k', capabilities: ['*'] }, owner);
		const call = async (method, path, body) => {
			const url = new URL(`http://localhost/api/v1${path}`);
			const init = { method, headers: { authorization: `Bearer ${key}`, 'content-type': 'application/json' } };
			if (body) init.body = JSON.stringify(body);
			const r = await handleApi({ request: new Request(url, init), url, locals: { container: c }, cookies: { get: () => undefined } }, url.pathname.slice('/api/v1'.length));
			return { status: r.status, body: await r.json() };
		};
		const sent = await call('POST', '/forms/newsletter/submissions', { email: 'api@example.com', consent: true });
		expect(sent.status).toBe(201);
		const list = await call('GET', '/submissions?form=' + (await forms(c).bySlug('newsletter')).id);
		expect(list.body.meta.total).toBe(1);
		expect((await call('PATCH', `/submissions/${list.body.data[0].id}`, { status: 'read' })).body.data.status).toBe('read');
		expect((await call('POST', '/forms', { name: 'Quote', fields: [{ label: 'Email', type: 'email', required: true }] })).status).toBe(201);
	});
});

describe('Aiosis Emails', () => {
	const real = globalThis.fetch;
	afterEach(() => {
		globalThis.fetch = real;
		delete process.env.AIOSIS_API_KEY;
	});

	test('sends POST /api/v1/messages with the sender from Settings → Email', async () => {
		const c = await setup();
		await settings(c).patch('mail', { fromEmail: 'hello@humansynthesis.ai', fromName: 'Human Synthesis', replyTo: '' }, null);
		process.env.AIOSIS_API_KEY = 'sk_live_test';
		let seen;
		globalThis.fetch = async (url, init) => {
			seen = { url: String(url), init };
			return new Response(JSON.stringify({ id: 'msg_1', status: 'queued' }), { status: 200, headers: { 'content-type': 'application/json' } });
		};
		const r = await mail(c).send({ to: 'team@example.com', subject: 'Hi', text: 'Body', replyTo: 'ada@example.com' });
		expect(r).toMatchObject({ ok: true, provider: 'aiosis', id: 'msg_1' });
		expect(seen.url).toBe('https://emails.aiosis.com/api/v1/messages');
		expect(seen.init.headers.Authorization).toBe('Bearer sk_live_test');
		const body = JSON.parse(seen.init.body);
		expect(body).toMatchObject({ to: 'team@example.com', subject: 'Hi', text: 'Body', fromEmail: 'hello@humansynthesis.ai', fromName: 'Human Synthesis', replyTo: 'ada@example.com' });
		expect(Object.keys(body).sort()).toEqual(['fromEmail', 'fromName', 'html', 'replyTo', 'subject', 'text', 'to']);
	});

	test('a refused recipient is logged as failed with the provider’s message', async () => {
		const c = await setup();
		process.env.AIOSIS_API_KEY = 'sk_live_test';
		globalThis.fetch = async () => new Response(JSON.stringify({ message: 'Destination address is not verified' }), { status: 400 });
		const r = await mail(c).send({ to: 'stranger@example.com', subject: 'Hi', text: 'Body' });
		expect(r).toMatchObject({ ok: false, error: 'Destination address is not verified' });
		const logged = await mail(c).lastTo('stranger@example.com');
		expect(logged.status).toBe('failed');
		expect(logged.error).toContain('not verified');
		expect(logged.body).toBe('');
	});
});
