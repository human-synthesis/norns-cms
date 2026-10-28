// Cloudflare Turnstile server-side check (siteverify). Tokens are single use
// and valid for 300 s; the widget puts one in the form as
// `cf-turnstile-response`.
export const SITEVERIFY = 'https://challenges.cloudflare.com/turnstile/v0/siteverify';

/**
 * @returns {Promise<{ success: boolean, errors: string[], hostname?: string, action?: string, unavailable?: boolean }>}
 */
export async function siteverify(secret, token, ip) {
	const body = new URLSearchParams({ secret, response: String(token ?? '') });
	if (ip && ip !== 'unknown') body.set('remoteip', ip);
	try {
		const res = await fetch(SITEVERIFY, { method: 'POST', body });
		if (!res.ok) return { success: false, errors: [`http-${res.status}`], unavailable: true };
		const r = await res.json();
		return { success: !!r.success, errors: r['error-codes'] ?? [], hostname: r.hostname, action: r.action };
	} catch {
		return { success: false, errors: ['network-error'], unavailable: true };
	}
}
