import { describe, test, expect } from 'bun:test';
import { bootApp, fakeEvent } from './helpers.js';
import { consent } from '../src/lib/norns/consent/server/public.c';
import { settings } from '../src/lib/norns/settings/server/public.c';
import { encodeConsent, decodeConsent, regionOf } from '../src/lib/norns/consent/shared/config.c';

const ga = { provider: 'ga4', name: 'GA4', category: 'analytics', config_measurementId: 'G-ABC1234', enabled: 'on' };

describe('consent state', () => {
	test('opt-in everywhere by default: nothing optional before a choice', async () => {
		const c = await bootApp();
		const s = await consent(c).state(fakeEvent({ country: 'US' }));
		expect(s.showBanner).toBe(true);
		expect(s.choice).toEqual({ preferences: false, analytics: false, marketing: false });
		expect(regionOf('DE')).toBe('eea');
		expect(regionOf('GB')).toBe('uk');
	});

	test('regional opt-out mode, with Global Privacy Control keeping marketing off', async () => {
		const c = await bootApp();
		await settings(c).patch('consent', { modeOther: 'opt-out' }, null);
		expect((await consent(c).state(fakeEvent({ country: 'US' }))).choice).toEqual({ preferences: true, analytics: true, marketing: true });
		expect((await consent(c).state(fakeEvent({ country: 'US', gpc: true }))).choice.marketing).toBe(false);
		expect((await consent(c).state(fakeEvent({ country: 'FR' }))).choice.analytics).toBe(false);
	});

	test('a stored choice applies until the policy version is raised', async () => {
		const c = await bootApp();
		const cookie = encodeConsent('a'.repeat(32), 1, { preferences: false, analytics: true, marketing: false });
		expect(decodeConsent(cookie).choice.analytics).toBe(true);
		const s = await consent(c).state(fakeEvent({ cookies: { cms_consent: cookie } }));
		expect(s.given).toBe(true);
		expect(s.choice.analytics).toBe(true);
		await consent(c).bumpVersion(null);
		const after = await consent(c).state(fakeEvent({ cookies: { cms_consent: cookie } }));
		expect(after.given).toBe(false);
		expect(after.choice.analytics).toBe(false);
		expect(decodeConsent('v1.not-hex.1.7.0')).toBeNull();
	});
});

describe('recording', () => {
	test('choices are logged as proof without IPs; the id stays stable', async () => {
		const c = await bootApp();
		const first = await consent(c).record(fakeEvent({ country: 'DE' }), { action: 'custom', analytics: 'on' }, null);
		expect(first.choice).toEqual({ preferences: false, analytics: true, marketing: false });
		const again = await consent(c).record(fakeEvent({ country: 'DE', cookies: { cms_consent: first.cookie } }), { action: 'withdraw' }, null);
		expect(again.consentId).toBe(first.consentId);
		const recs = await consent(c).records({ consentId: first.consentId });
		expect(recs.map((r) => r.action)).toEqual(['withdraw', 'custom']);
		expect(recs[1].country).toBe('DE');
		expect(recs[1].region_mode).toBe('opt-in');
		expect(Object.keys(recs[1])).not.toContain('ip');
		const stats = await consent(c).stats();
		expect(stats.total).toBe(2);
		expect(stats.custom).toBe(1);
	});

	test('accept all respects GPC for marketing', async () => {
		const c = await bootApp();
		const r = await consent(c).record(fakeEvent({ gpc: true }), { action: 'accept_all' }, null);
		expect(r.choice).toEqual({ preferences: true, analytics: true, marketing: false });
	});
});

describe('scripts', () => {
	test('preset configs are validated against strict patterns', async () => {
		const c = await bootApp();
		const bad = await consent(c).saveScript(null, { ...ga, config_measurementId: 'G-1"><script>' }, null);
		expect(bad.ok).toBe(false);
		expect(bad.field).toBe('config_measurementId');
		expect((await consent(c).saveScript(null, ga, null)).ok).toBe(true);
	});

	test('scripts without consent are inert templates; with consent they are live', async () => {
		const c = await bootApp();
		await consent(c).saveScript(null, ga, null);
		await consent(c).saveScript(null, { provider: 'meta_pixel', category: 'marketing', config_pixelId: '123456789012', enabled: 'on' }, null);
		const none = await consent(c).injection(await consent(c).state(fakeEvent()), 'NONCE');
		expect(none.head).toContain(`gtag('consent','default'`);
		expect(none.head).toContain('"analytics_storage":"denied"');
		expect(none.head).toContain('<template data-consent="analytics"><script async src="https://www.googletagmanager.com/gtag/js?id=G-ABC1234">');
		expect(none.head).toContain('<template data-consent="marketing">');
		expect(none.body).toContain('<script nonce="NONCE">window.__cmsConsent={"preferences":false,"analytics":false,"marketing":false}');
		const cookie = encodeConsent('b'.repeat(32), 1, { preferences: true, analytics: true, marketing: false });
		const some = await consent(c).injection(await consent(c).state(fakeEvent({ cookies: { cms_consent: cookie } })), 'N');
		expect(some.head).toContain('"analytics_storage":"granted"');
		expect(some.head).not.toContain('<template data-consent="analytics">');
		expect(some.head).toContain('<template data-consent="marketing">');
	});

	test('the declaration lists necessary cookies and each enabled service', async () => {
		const c = await bootApp();
		await consent(c).saveScript(null, ga, null);
		await consent(c).saveScript(null, { provider: 'custom', name: 'Chat widget', category: 'preferences', code: '<script>1</script>', cookies: 'chat_id | 1 year | Remembers the conversation', enabled: 'on' }, null);
		const d = await consent(c).declaration();
		const by = Object.fromEntries(d.groups.map((g) => [g.id, g]));
		expect(by.necessary.cookies.map((k) => k.name)).toContain('cms_consent');
		expect(by.analytics.services).toEqual(['GA4']);
		expect(by.analytics.cookies.map((k) => k.name)).toContain('_ga');
		expect(by.preferences.cookies[0]).toEqual({ name: 'chat_id', duration: '1 year', purpose: 'Remembers the conversation', service: 'Chat widget' });
		expect(d.patterns.analytics).toContain('_ga_*');
	});
});
