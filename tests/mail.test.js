import { describe, test, expect, afterEach } from 'bun:test';
import { bootApp } from './helpers.js';
import { mail } from '../src/lib/norns/mail/server/public.c';
import { settings } from '../src/lib/norns/settings/server/public.c';
import { forms } from '../src/lib/norns/forms/server/public.c';

const realFetch = globalThis.fetch;
const env = { ...process.env };
afterEach(() => {
	globalThis.fetch = realFetch;
	for (const k of ['AIOSIS_API_KEY', 'NODE_ENV', 'MAIL_LOG_ONLY', 'MAIL_DOMAIN']) {
		if (env[k] === undefined) delete process.env[k];
		else process.env[k] = env[k];
	}
});

function capture() {
	const sent = [];
	globalThis.fetch = async (url, init) => {
		sent.push({ url: String(url), headers: init.headers, body: JSON.parse(init.body) });
		return new Response(JSON.stringify({ id: `msg_${sent.length}`, status: 'queued' }), { status: 200 });
	};
	return sent;
}

describe('Aiosis Emails', () => {
	test('each purpose sends from its own address on the mail domain', async () => {
		const c = await bootApp();
		await settings(c).patch('mail', { domain: 'example.org' }, null);
		process.env.AIOSIS_API_KEY = 'sk_test_x';
		const sent = capture();
		for (const purpose of ['signup', 'signin', 'notifications', 'contact']) {
			const r = await mail(c).send({ purpose, to: 'a@example.com', subject: purpose, text: 'Hi' });
			expect(r).toMatchObject({ ok: true, provider: 'aiosis' });
		}
		expect(sent.map((s) => s.body.fromEmail)).toEqual(['signup@example.org', 'signin@example.org', 'notification@example.org', 'contact@example.org']);
		expect(sent[0].url).toBe('https://emails.aiosis.com/api/v1/messages');
		expect(sent[0].headers.Authorization).toBe('Bearer sk_test_x');
	});

	test('a full address set per purpose wins over the domain', async () => {
		const c = await bootApp();
		await settings(c).patch('mail', { domain: 'example.org', contactFrom: 'hello@example.com' }, null);
		process.env.AIOSIS_API_KEY = 'sk_test_x';
		const sent = capture();
		await mail(c).send({ purpose: 'contact', to: 'a@example.com', subject: 'x', text: 'y' });
		await mail(c).send({ purpose: 'signup', to: 'a@example.com', subject: 'x', text: 'y' });
		expect(sent.map((s) => s.body.fromEmail)).toEqual(['hello@example.com', 'signup@example.org']);
	});

	test('MAIL_DOMAIN is the fallback domain', async () => {
		const c = await bootApp();
		process.env.AIOSIS_API_KEY = 'sk_test_x';
		process.env.MAIL_DOMAIN = 'mail.example.net';
		const sent = capture();
		await mail(c).send({ purpose: 'signin', to: 'a@example.com', subject: 'x', text: 'y' });
		expect(sent[0].body.fromEmail).toBe('signin@mail.example.net');
	});

	test('auth and forms pick the right sender', async () => {
		const c = await bootApp();
		await settings(c).patch('mail', { domain: 'example.org', notifyTo: ['team@example.com'] }, null);
		process.env.AIOSIS_API_KEY = 'sk_test_x';
		const sent = capture();
		await forms(c).submit('contact', { _t: String(Date.now() - 5000), _hp: '', name: 'Ada', email: 'ada@example.com', message: 'Hello', consent: 'on' }, { lang: 'en', page: '/contact', ip: '10.9.0.1' });
		await forms(c).submit('newsletter', { _t: String(Date.now() - 5000), _hp: '', email: 'n@example.com', consent: 'on' }, { lang: 'en', page: '/', ip: '10.9.0.2' });
		expect(sent.map((s) => s.body.fromEmail)).toEqual(['contact@example.org', 'notification@example.org']);
		expect(sent[0].body.replyTo).toBe('ada@example.com');
	});

	test('the key is required outside dev and tests', async () => {
		const c = await bootApp();
		delete process.env.AIOSIS_API_KEY;
		process.env.NODE_ENV = 'production';
		const sent = capture();
		expect(mail(c).provider()).toBe('missing');
		const r = await mail(c).send({ purpose: 'signup', to: 'a@example.com', subject: 'x', text: 'y' });
		expect(r).toMatchObject({ ok: false, provider: 'missing', error: 'AIOSIS_API_KEY is not set' });
		expect(sent).toEqual([]);
		expect(await mail(c).lastTo('a@example.com')).toMatchObject({ status: 'failed', provider: 'missing' });
		process.env.MAIL_LOG_ONLY = 'true';
		expect(mail(c).provider()).toBe('log');
	});
});
