import { describe, test, expect } from 'bun:test';
import { bootApp } from './helpers.js';
import { auth, can } from '../src/lib/norns/auth/server/public.c';
import { mail } from '../src/lib/norns/mail/server/public.c';
import { settings } from '../src/lib/norns/settings/server/public.c';
import { totp } from '../src/lib/norns/auth/server/totp.js';

const PW = 'correct horse battery';
const linkFrom = async (c, email, kind) => {
	const m = await mail(c).lastTo(email);
	return new RegExp(`/${kind}/([A-Za-z0-9_-]+)`).exec(m.body)[1];
};
const signIn = async (c, email, pw = PW) => {
	const r = await auth(c).login(email, pw, { ip: '1.2.3.4' });
	return r.ok ? r.user : r;
};

describe('sign-in', () => {
	test('login resolves a session with role capabilities; logout revokes it', async () => {
		const c = await bootApp();
		await auth(c).createUser('Admin@Example.com', 'Ada', PW, 'owner');
		expect((await auth(c).login('admin@example.com', 'wrong', { ip: '1.1.1.1' })).error).toBe('invalid');
		const r = await auth(c).login('admin@example.com', PW, { ip: '1.1.1.1' });
		expect(r.ok).toBe(true);
		expect(r.user.capabilities).toEqual(['*']);
		await auth(c).logout(r.session.id);
		expect(await auth(c).userForSession(r.session.id)).toBeUndefined();
	});

	test('five failures lock an email out for the window', async () => {
		const c = await bootApp();
		await auth(c).createUser('a@example.com', 'A', PW, 'editor');
		for (let i = 0; i < 5; i++) await auth(c).login('a@example.com', 'nope', { ip: `9.9.9.${i}` });
		expect((await auth(c).login('a@example.com', PW, { ip: '8.8.8.8' })).error).toBe('rate');
	});

	test('capabilities: wildcards and role boundaries', async () => {
		const c = await bootApp();
		const editor = await auth(c).role('editor');
		const author = await auth(c).role('author');
		const u = (role) => ({ status: 'active', capabilities: role.capabilities });
		expect(can(u(editor), 'content.publish')).toBe(true);
		expect(can(u(editor), 'users.manage')).toBe(false);
		expect(can(u(author), 'content.edit_own')).toBe(true);
		expect(can(u(author), 'content.publish')).toBe(false);
		expect(can({ status: 'suspended', capabilities: ['*'] }, 'content.view')).toBe(false);
	});

	test('a suspended user loses their sessions', async () => {
		const c = await bootApp();
		const ownerId = await auth(c).createUser('o@example.com', 'O', PW, 'owner');
		const owner = await auth(c).getUser(ownerId);
		const id = await auth(c).createUser('e@example.com', 'E', PW, 'editor');
		const r = await auth(c).login('e@example.com', PW, { ip: '1.1.1.1' });
		expect((await auth(c).updateUser(id, { status: 'suspended' }, owner)).ok).toBe(true);
		expect(await auth(c).userForSession(r.session.id)).toBeUndefined();
		expect((await auth(c).login('e@example.com', PW, { ip: '1.1.1.1' })).ok).toBe(false);
	});
});

describe('owner rules', () => {
	test('only owners manage owners; the last owner stays; nobody demotes themselves', async () => {
		const c = await bootApp();
		const o = await auth(c).getUser(await auth(c).createUser('o@example.com', 'O', PW, 'owner'));
		const a = await auth(c).getUser(await auth(c).createUser('a@example.com', 'A', PW, 'admin'));
		expect((await auth(c).updateUser(o.id, { role: 'editor' }, a)).error).toBe('Only an owner can change owners');
		expect((await auth(c).updateUser(a.id, { role: 'owner' }, a)).ok).toBe(false);
		expect((await auth(c).updateUser(o.id, { role: 'editor' }, o)).error).toBe('You cannot change your own role or status');
		expect((await auth(c).updateUser(a.id, { role: 'owner' }, o)).ok).toBe(true);
		expect((await auth(c).updateUser(a.id, { role: 'admin' }, o)).ok).toBe(true);
		expect((await auth(c).deleteUser(o.id, o)).ok).toBe(false);
		expect((await auth(c).updateUser(a.id, { role: 'member' }, o)).error).toBe('That role is for a different kind of user');
	});
});

describe('invites, resets, registration', () => {
	test('an invite sets the password and activates the account', async () => {
		const c = await bootApp();
		const o = await auth(c).getUser(await auth(c).createUser('o@example.com', 'O', PW, 'owner'));
		const r = await auth(c).invite({ email: 'New@Example.com', name: '', role: 'author' }, o);
		expect(r.ok).toBe(true);
		expect(r.link).toContain('/invite/');
		expect((await auth(c).login('new@example.com', PW, { ip: '2.2.2.2' })).ok).toBe(false);
		const token = await linkFrom(c, 'new@example.com', 'invite');
		expect((await auth(c).acceptInvite(token, 'Nia', 'short')).field).toBe('password');
		expect((await auth(c).acceptInvite(token, 'Nia New', PW)).ok).toBe(true);
		expect((await auth(c).acceptInvite(token, 'Nia New', PW)).ok).toBe(false);
		const u = await signIn(c, 'new@example.com');
		expect(u.role).toBe('author');
		expect(u.slug).toBe('nia-new');
	});

	test('password reset works once and signs out everywhere', async () => {
		const c = await bootApp();
		await auth(c).createUser('r@example.com', 'R', PW, 'editor');
		const before = await auth(c).login('r@example.com', PW, { ip: '3.3.3.3' });
		expect((await auth(c).requestReset('nobody@example.com')).ok).toBe(true);
		await auth(c).requestReset('r@example.com', '3.3.3.3');
		const token = await linkFrom(c, 'r@example.com', 'reset');
		expect((await auth(c).resetPassword(token, 'a brand new passphrase')).ok).toBe(true);
		expect((await auth(c).resetPassword(token, 'another new passphrase')).ok).toBe(false);
		expect(await auth(c).userForSession(before.session.id)).toBeUndefined();
		expect((await auth(c).login('r@example.com', 'a brand new passphrase', { ip: '3.3.3.3' })).ok).toBe(true);
	});

	test('registration follows the site setting and needs a verified email', async () => {
		const c = await bootApp();
		expect((await auth(c).register({ email: 'm@example.com', name: 'M', password: PW })).ok).toBe(false);
		await settings(c).patch('site', { registration: 'open' }, null);
		expect((await auth(c).register({ email: 'm@example.com', name: 'M', password: PW }, '5.5.5.5')).ok).toBe(true);
		expect((await auth(c).login('m@example.com', PW, { ip: '5.5.5.5' })).error).toBe('unverified');
		const token = await linkFrom(c, 'm@example.com', 'verify');
		expect((await auth(c).verifyEmail(token)).ok).toBe(true);
		const m = await signIn(c, 'm@example.com');
		expect(m.kind).toBe('client');
		expect(can(m, 'admin.access')).toBe(false);
		expect(can(m, 'content.members')).toBe(true);
		// Registering an existing email looks the same but mails the owner.
		expect((await auth(c).register({ email: 'm@example.com', name: 'X', password: PW }, '5.5.5.6')).ok).toBe(true);
		expect((await mail(c).lastTo('m@example.com')).template).toBe('register_exists');
	});

	test('clients can delete their own account; it is anonymised at once', async () => {
		const c = await bootApp();
		const id = await auth(c).createUser('gone@example.com', 'G', PW, 'member', { kind: 'client' });
		const u = await signIn(c, 'gone@example.com');
		expect((await auth(c).deleteOwnAccount(u, 'wrong')).ok).toBe(false);
		expect((await auth(c).deleteOwnAccount(u, PW)).ok).toBe(true);
		expect(await auth(c).userByEmail('gone@example.com')).toBeUndefined();
		expect((await auth(c).getUser(id)).status).toBe('deleted');
	});
});

describe('two-factor', () => {
	test('enrol, then sign-in needs a code; recovery codes work once', async () => {
		const c = await bootApp();
		await auth(c).createUser('t@example.com', 'T', PW, 'owner');
		const u = await signIn(c, 't@example.com');
		const { secret } = await auth(c).beginTotp(u);
		expect((await auth(c).confirmTotp(u, '000000')).ok).toBe(false);
		const done = await auth(c).confirmTotp(u, totp(secret));
		expect(done.codes).toHaveLength(8);
		const step = await auth(c).login('t@example.com', PW, { ip: '4.4.4.4' });
		expect(step.ok).toBe(false);
		expect(step.mfa).toBeTruthy();
		expect((await auth(c).loginMfa(step.mfa, '123456')).ok).toBe(false);
		expect((await auth(c).loginMfa(step.mfa, totp(secret))).ok).toBe(true);
		const again = await auth(c).login('t@example.com', PW, { ip: '4.4.4.4' });
		expect((await auth(c).loginMfa(again.mfa, done.codes[0])).ok).toBe(true);
		const third = await auth(c).login('t@example.com', PW, { ip: '4.4.4.4' });
		expect((await auth(c).loginMfa(third.mfa, done.codes[0])).ok).toBe(false);
	});

	test('require2fa flags owners and admins without it', async () => {
		const c = await bootApp();
		await auth(c).createUser('o@example.com', 'O', PW, 'owner');
		await settings(c).patch('security', { require2fa: true }, null);
		expect((await signIn(c, 'o@example.com')).needs2fa).toBe(true);
	});
});

describe('roles and audit', () => {
	test('custom roles keep only declared capabilities; built-ins are fixed', async () => {
		const c = await bootApp();
		const o = await auth(c).getUser(await auth(c).createUser('o@example.com', 'O', PW, 'owner'));
		const r = await auth(c).saveRole({ kind: 'staff', label: 'SEO specialist', description: '', capabilities: ['seo.manage', 'bogus.cap'] }, o);
		expect(r.id).toBe('seo_specialist');
		expect((await auth(c).role('seo_specialist')).capabilities).toEqual(['admin.access', 'seo.manage']);
		expect((await auth(c).saveRole({ id: 'editor', kind: 'staff', label: 'Editor', capabilities: [] }, o)).ok).toBe(false);
		await auth(c).createUser('s@example.com', 'S', PW, 'seo_specialist');
		expect((await auth(c).deleteRole('seo_specialist', o)).ok).toBe(false);
	});

	test('security-relevant actions are audited', async () => {
		const c = await bootApp();
		await auth(c).createUser('o@example.com', 'O', PW, 'owner');
		await auth(c).login('o@example.com', 'bad', { ip: '6.6.6.6' });
		await auth(c).login('o@example.com', PW, { ip: '6.6.6.6' });
		const { audit } = await import('../src/lib/norns/auth/server/public.c');
		const actions = (await audit(c).list()).map((a) => a.action);
		expect(actions).toEqual(['login', 'login.failed']);
	});
});
