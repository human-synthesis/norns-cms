// Secrets kept in the database (API secrets people paste into the admin),
// sealed with AES-256-GCM under a key derived from CMS_SECRET and a purpose
// string, so one leaked purpose key opens nothing else. Without CMS_SECRET
// (local dev) values are stored as `plain:<value>`; set it in production.
import { createHash, createHmac, randomBytes, createCipheriv, createDecipheriv } from 'node:crypto';
import { envVar } from './env.js';

const keyFor = (purpose) => createHash('sha256').update(`norns-cms:${purpose}:${envVar('CMS_SECRET')}`).digest();

/** @param {string} plain @param {string} purpose */
export function sealValue(plain, purpose) {
	if (!envVar('CMS_SECRET')) return `plain:${plain}`;
	const iv = randomBytes(12);
	const cipher = createCipheriv('aes-256-gcm', keyFor(purpose), iv);
	const ct = Buffer.concat([cipher.update(String(plain), 'utf8'), cipher.final()]);
	return `v1:${iv.toString('base64')}:${cipher.getAuthTag().toString('base64')}:${ct.toString('base64')}`;
}

/** @param {string | null | undefined} sealed @param {string} purpose */
export function openValue(sealed, purpose) {
	if (!sealed) return null;
	if (sealed.startsWith('plain:')) return sealed.slice(6);
	const [v, iv, tag, ct] = sealed.split(':');
	if (v !== 'v1' || !envVar('CMS_SECRET')) return null;
	try {
		const d = createDecipheriv('aes-256-gcm', keyFor(purpose), Buffer.from(iv, 'base64'));
		d.setAuthTag(Buffer.from(tag, 'base64'));
		return Buffer.concat([d.update(Buffer.from(ct, 'base64')), d.final()]).toString('utf8');
	} catch {
		return null;
	}
}

/** HMAC-SHA256 (hex) under CMS_SECRET, for signed tokens that are not secret. */
export function signValue(value, purpose) {
	return createHmac('sha256', `norns-cms:${purpose}:${envVar('CMS_SECRET') ?? 'dev'}`).update(String(value)).digest('hex');
}
