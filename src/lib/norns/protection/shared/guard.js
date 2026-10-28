// Field names and labels shared by the server check and FormGuard.n.
export const HONEYPOT = '_hp';
export const TOKEN = '_g';
export const TURNSTILE_FIELD = 'cf-turnstile-response';

export const OUTCOMES = {
	passed: 'Passed',
	honeypot: 'Honeypot filled',
	too_fast: 'Sent too fast',
	token: 'Missing or expired page token',
	turnstile: 'Turnstile failed',
	unverified: 'Let through: Cloudflare unreachable'
};
export const BLOCKED = ['honeypot', 'too_fast', 'token', 'turnstile'];

/** Turnstile `action`: up to 32 of [A-Za-z0-9_-]. */
export const actionFor = (formId) => String(formId).replace(/[^A-Za-z0-9_-]/g, '_').slice(0, 32);

/** What applies to one form: its override if any, else its default; Turnstile only when configured. */
export function decide(def, override, active) {
	const mode = override?.turnstile ?? 'default';
	const wants = mode === 'on' ? true : mode === 'off' ? false : def?.turnstile !== false;
	return { turnstile: !!active && wants, honeypot: override ? !!override.honeypot : def?.honeypot !== false };
}
