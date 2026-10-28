// TOTP (RFC 6238, SHA-1, 6 digits, 30 s) for two-factor sign-in, plus
// at-rest encryption of the shared secret with AES-256-GCM keyed from the
// CMS_SECRET env var. Without CMS_SECRET the secret is stored as
// `plain:<base32>` (dev); set the secret in production.
import { createHmac, createHash, randomBytes, createCipheriv, createDecipheriv, timingSafeEqual } from 'node:crypto';

const ALPHABET = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ234567';

export function base32Encode(buf) {
	let bits = 0;
	let value = 0;
	let out = '';
	for (const byte of buf) {
		value = (value << 8) | byte;
		bits += 8;
		while (bits >= 5) {
			out += ALPHABET[(value >>> (bits - 5)) & 31];
			bits -= 5;
		}
	}
	if (bits > 0) out += ALPHABET[(value << (5 - bits)) & 31];
	return out;
}

export function base32Decode(s) {
	const clean = String(s).toUpperCase().replace(/[^A-Z2-7]/g, '');
	let bits = 0;
	let value = 0;
	const out = [];
	for (const ch of clean) {
		value = (value << 5) | ALPHABET.indexOf(ch);
		bits += 5;
		if (bits >= 8) {
			out.push((value >>> (bits - 8)) & 255);
			bits -= 8;
		}
	}
	return Buffer.from(out);
}

export function newSecret() {
	return base32Encode(randomBytes(20));
}

export function hotp(secret, counter) {
	const buf = Buffer.alloc(8);
	buf.writeBigUInt64BE(BigInt(counter));
	const h = createHmac('sha1', base32Decode(secret)).update(buf).digest();
	const offset = h[h.length - 1] & 15;
	const code = ((h[offset] & 127) << 24) | (h[offset + 1] << 16) | (h[offset + 2] << 8) | h[offset + 3];
	return String(code % 1_000_000).padStart(6, '0');
}

export function totp(secret, at = Date.now()) {
	return hotp(secret, Math.floor(at / 30000));
}

/** Accepts the current step and one step either side (clock drift). */
export function verifyTotp(secret, code, at = Date.now()) {
	const c = String(code ?? '').replace(/\s/g, '');
	if (!/^\d{6}$/.test(c)) return false;
	const step = Math.floor(at / 30000);
	for (const d of [-1, 0, 1]) {
		const expected = Buffer.from(hotp(secret, step + d));
		if (timingSafeEqual(expected, Buffer.from(c))) return true;
	}
	return false;
}

export function otpauthUri(secret, account, issuer) {
	return `otpauth://totp/${encodeURIComponent(issuer)}:${encodeURIComponent(account)}?secret=${secret}&issuer=${encodeURIComponent(issuer)}&algorithm=SHA1&digits=6&period=30`;
}

const keyFrom = (secret) => createHash('sha256').update(`norns-cms:totp:${secret}`).digest();

export function sealSecret(plain, appSecret) {
	if (!appSecret) return `plain:${plain}`;
	const iv = randomBytes(12);
	const cipher = createCipheriv('aes-256-gcm', keyFrom(appSecret), iv);
	const ct = Buffer.concat([cipher.update(plain, 'utf8'), cipher.final()]);
	return `v1:${iv.toString('base64')}:${cipher.getAuthTag().toString('base64')}:${ct.toString('base64')}`;
}

export function openSecret(sealed, appSecret) {
	if (!sealed) return null;
	if (sealed.startsWith('plain:')) return sealed.slice(6);
	const [v, iv, tag, ct] = sealed.split(':');
	if (v !== 'v1' || !appSecret) return null;
	const d = createDecipheriv('aes-256-gcm', keyFrom(appSecret), Buffer.from(iv, 'base64'));
	d.setAuthTag(Buffer.from(tag, 'base64'));
	return Buffer.concat([d.update(Buffer.from(ct, 'base64')), d.final()]).toString('utf8');
}

export const sha256 = (s) => createHash('sha256').update(String(s)).digest('hex');

export function recoveryCodes(n = 8) {
	return Array.from({ length: n }, () => {
		const raw = randomBytes(5).toString('hex');
		return `${raw.slice(0, 5)}-${raw.slice(5)}`;
	});
}
