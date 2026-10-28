import { describe, test, expect } from 'bun:test';
import { bootApp } from './helpers.js';
import { auth, can } from '../src/lib/norns/auth/server/public.c';
import { settings } from '../src/lib/norns/settings/server/public.c';
import { twoFactor } from '../src/lib/norns/twofactor/server/public.c';
import { apiKeys, handleApi } from '../src/lib/norns/api/server/public.c';
import { totp } from '../src/lib/norns/twofactor/server/totp.js';

const PW = 'correct horse battery';
const signIn = async (c, email, pw = PW) => {
	const r = await auth(c).login(email, pw, { ip: '1.2.3.4' });
	return r.ok ? r.user : r;
};
const enrol = async (c, user) => {
	const { secret } = await twoFactor(c).begin(user);
	const done = await twoFactor(c).confirm(user, totp(secret));
	return { secret, codes: done.codes };
};

describe('setup', () => {
	test('setup gives a QR code of the otpauth link with the site as issuer', async () => {
		const c = await bootApp();
		await auth(c).createUser('t@example.com', 'T', PW, 'owner');
		await settings(c).patch('site', { title: 'Acme Site' }, null);
		const u = await signIn(c, 't@example.com');
		const setup = await twoFactor(c).begin(u);
		expect(setup.uri).toBe(`otpauth://totp/Acme%20Site:t%40example.com?secret=${setup.secret}&issuer=Acme%20Site&algorithm=SHA1&digits=6&period=30`);
		expect(setup.qr).toStartWith('<svg');
		expect(setup.qr).toContain('<path');
		// After a wrong code the same setup is shown again; nothing is on yet.
		expect(await twoFactor(c).confirm(u, '000000')).toEqual({ ok: false, error: 'twofactor.wrongCode' });
		expect((await twoFactor(c).pending(u)).secret).toBe(setup.secret);
		expect((await twoFactor(c).status(u)).enabled).toBe(false);
	});

	test('enrol, then sign-in needs a code; recovery codes work once', async () => {
		const c = await bootApp();
		await auth(c).createUser('t@example.com', 'T', PW, 'owner');
		const u = await signIn(c, 't@example.com');
		const { secret, codes } = await enrol(c, u);
		expect(codes).toHaveLength(8);
		expect(await twoFactor(c).pending(u)).toBeNull();
		const step = await auth(c).login('t@example.com', PW, { ip: '4.4.4.4' });
		expect(step.ok).toBe(false);
		expect(step.mfa).toBeTruthy();
		expect(step.kind).toBe('staff');
		expect((await auth(c).loginMfa(step.mfa, '123456')).ok).toBe(false);
		expect((await auth(c).loginMfa(step.mfa, totp(secret))).ok).toBe(true);
		const again = await auth(c).login('t@example.com', PW, { ip: '4.4.4.4' });
		expect((await auth(c).loginMfa(again.mfa, codes[0])).ok).toBe(true);
		expect((await twoFactor(c).status(u)).recoveryCodesLeft).toBe(7);
		const third = await auth(c).login('t@example.com', PW, { ip: '4.4.4.4' });
		expect((await auth(c).loginMfa(third.mfa, codes[0])).ok).toBe(false);
	});

	test('turning it off needs the password; a reset follows the owner rules', async () => {
		const c = await bootApp();
		await auth(c).createUser('o@example.com', 'O', PW, 'owner');
		const edId = await auth(c).createUser('e@example.com', 'E', PW, 'admin');
		const o = await signIn(c, 'o@example.com');
		const ed = await signIn(c, 'e@example.com');
		await enrol(c, o);
		expect((await twoFactor(c).disable(o, 'wrong')).error).toBe('twofactor.wrongPassword');
		expect((await twoFactor(c).reset(o.id, ed)).ok).toBe(false); // an admin cannot touch an owner
		await enrol(c, ed);
		expect((await twoFactor(c).reset(edId, o)).ok).toBe(true);
		expect(await twoFactor(c).enabled(edId)).toBe(false);
		expect((await twoFactor(c).disable(o, PW)).ok).toBe(true);
		expect((await auth(c).login('o@example.com', PW, { ip: '4.4.4.4' })).ok).toBe(true);
	});
});

describe('policies: CMS staff and members are separate', () => {
	test('staff policy: admins only, or all staff; members untouched', async () => {
		const c = await bootApp();
		await auth(c).createUser('o@example.com', 'O', PW, 'owner');
		await auth(c).createUser('e@example.com', 'E', PW, 'editor');
		await auth(c).createUser('m@example.com', 'M', PW, 'member', { kind: 'client' });
		expect((await signIn(c, 'o@example.com')).needs2fa).toBe(false);
		await settings(c).patch('twofactor', { staff: 'admins' }, null);
		expect((await signIn(c, 'o@example.com')).needs2fa).toBe(true);
		expect((await signIn(c, 'e@example.com')).needs2fa).toBe(false);
		await settings(c).patch('twofactor', { staff: 'staff' }, null);
		const ed = await signIn(c, 'e@example.com');
		expect(ed.needs2fa).toBe(true);
		expect((await twoFactor(c).disable(ed, PW)).ok).toBe(false);
		const m = await signIn(c, 'm@example.com');
		expect(m.needs2fa).toBe(false);
		expect((await twoFactor(c).status(m)).policy).toBe('off');
	});

	test('members: optional, or required with member features held back until they enrol', async () => {
		const c = await bootApp();
		await auth(c).createUser('m@example.com', 'M', PW, 'member', { kind: 'client' });
		await auth(c).createUser('o@example.com', 'O', PW, 'owner');
		await settings(c).patch('twofactor', { members: 'optional' }, null);
		let m = await signIn(c, 'm@example.com');
		expect((await twoFactor(c).status(m)).policy).toBe('optional');
		expect(can(m, 'content.members')).toBe(true);
		await settings(c).patch('twofactor', { members: 'required' }, null);
		m = await signIn(c, 'm@example.com');
		expect(m.needs2fa).toBe(true);
		expect(m.capabilities).toEqual(['account.access']);
		expect(can(m, 'content.members')).toBe(false);
		// The staff side is unaffected by the members' policy.
		expect((await signIn(c, 'o@example.com')).needs2fa).toBe(false);
		const { secret } = await enrol(c, m);
		const step = await auth(c).login('m@example.com', PW, { ip: '4.4.4.4' });
		expect(step.kind).toBe('client');
		const done = await auth(c).loginMfa(step.mfa, totp(secret));
		expect(done.user.needs2fa).toBe(false);
		expect(can(done.user, 'content.members')).toBe(true);
	});
});

describe('migration', () => {
	test('two-factor kept in the old users columns carries over', async () => {
		const c = await bootApp();
		const id = await auth(c).createUser('old@example.com', 'Old', PW, 'owner');
		const db = c.resolve('db');
		// Simulate a pre-migration row, then re-run the feature's migration.
		await db.run('DELETE FROM two_factor', []);
		await db.run("UPDATE users SET totp_secret = 'plain:JBSWY3DPEHPK3PXP', totp_enabled = 1, recovery_codes = '[]' WHERE id = ?", [id]);
		const { readFileSync } = await import('node:fs');
		await db.exec(readFileSync(new URL('../migrations/twofactor/20261028_002_init.sql', import.meta.url), 'utf8'));
		expect(await twoFactor(c).enabled(id)).toBe(true);
		const step = await auth(c).login('old@example.com', PW, { ip: '4.4.4.4' });
		expect((await auth(c).loginMfa(step.mfa, totp('JBSWY3DPEHPK3PXP'))).ok).toBe(true);
	});
});

describe('API', () => {
	test('status and reset', async () => {
		const c = await bootApp();
		const ownerId = await auth(c).createUser('owner@example.com', 'Olga', PW, 'owner');
		const owner = { ...(await auth(c).getUser(ownerId)), capabilities: ['*'] };
		const edId = await auth(c).createUser('e@example.com', 'E', PW, 'editor');
		await enrol(c, await signIn(c, 'e@example.com'));
		const { key } = await apiKeys(c).create({ name: 'Ops', capabilities: ['users.view', 'users.manage'] }, owner);
		const call = (method, path) => {
			const url = new URL(`http://localhost/api/v1${path}`);
			const event = { request: new Request(url, { method, headers: { authorization: `Bearer ${key}` } }), url, locals: { container: c }, cookies: { get: () => undefined }, getClientAddress: () => '127.0.0.1' };
			return handleApi(event, path).then(async (r) => ({ status: r.status, body: await r.json() }));
		};
		const s = await call('GET', `/users/${edId}/2fa`);
		expect(s.status).toBe(200);
		expect(s.body.data).toMatchObject({ enabled: true, recoveryCodesLeft: 8, policy: 'optional' });
		expect(JSON.stringify(s.body)).not.toContain('secret');
		expect((await call('POST', `/users/${edId}/2fa/reset`)).status).toBe(200);
		expect((await call('GET', `/users/${edId}/2fa`)).body.data.enabled).toBe(false);
		expect((await call('GET', '/users/9999/2fa')).status).toBe(404);
	});
});
