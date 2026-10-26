// Password policy for new passwords (sign-in accepts any existing one).
const COMMON = new Set(
	`password password1 password123 123456789012 qwertyuiopas iloveyou1234 letmein12345 welcome12345
	adminadmin12 administrator changeme1234 passw0rd1234 qwerty123456 1q2w3e4r5t6y abc123456789
	000000000000 111111111111 123123123123 monkey123456 dragon123456 football1234 baseball1234
	sunshine1234 princess1234 trustno11234 superman1234 starwars1234 password1234 correcthorse
	correcthorsebatterystaple`.split(/\s+/)
);

export const MIN_PASSWORD = 12;

/** @returns {string | null} an error message, or null when acceptable */
export function passwordProblem(password, email = '') {
	const p = String(password ?? '');
	if (p.length < MIN_PASSWORD) return `Use at least ${MIN_PASSWORD} characters`;
	if (p.length > 200) return 'That password is too long';
	if (COMMON.has(p.toLowerCase())) return 'That password is too common';
	if (/^(.)\1+$/.test(p)) return 'That password is too easy to guess';
	const local = String(email).split('@')[0]?.toLowerCase();
	if (local && local.length >= 4 && p.toLowerCase().includes(local)) return 'Do not include your email address';
	return null;
}
