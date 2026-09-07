// Password hashing with node:crypto scrypt (no bcrypt/argon2 — see CLAUDE.md).
// Stored form: `scrypt$<salt hex>$<hash hex>`.
import { randomBytes, scryptSync, timingSafeEqual } from 'node:crypto';

const KEYLEN = 64;
const OPTS = { N: 16384, r: 8, p: 1 };

/** @param {string} password */
export function hashPassword(password) {
	const salt = randomBytes(16).toString('hex');
	const hash = scryptSync(password, salt, KEYLEN, OPTS).toString('hex');
	return `scrypt$${salt}$${hash}`;
}

/**
 * @param {string} password
 * @param {string} stored
 */
export function verifyPassword(password, stored) {
	if (typeof stored !== 'string') return false;
	const [algo, salt, hash] = stored.split('$');
	if (algo !== 'scrypt' || !salt || !hash) return false;
	const candidate = scryptSync(password, salt, KEYLEN, OPTS);
	const expected = Buffer.from(hash, 'hex');
	return candidate.length === expected.length && timingSafeEqual(candidate, expected);
}
