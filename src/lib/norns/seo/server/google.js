// Google service-account access token (OAuth 2.0 JWT bearer grant) with
// WebCrypto only, so it runs on Workers and under Bun. `json` is the
// service account's key file (GOOGLE_SERVICE_ACCOUNT secret).

const TOKEN_URL = 'https://oauth2.googleapis.com/token';

const b64url = (bytes) =>
	btoa(String.fromCharCode(...new Uint8Array(bytes)))
		.replace(/\+/g, '-')
		.replace(/\//g, '_')
		.replace(/=+$/, '');
const b64urlText = (s) => b64url(new TextEncoder().encode(s));

/** @returns {{ client_email: string, private_key: string, token_uri?: string }} */
export function parseServiceAccount(json) {
	let sa;
	try {
		sa = typeof json === 'string' ? JSON.parse(json) : json;
	} catch {
		throw new Error('GOOGLE_SERVICE_ACCOUNT is not valid JSON');
	}
	if (!sa?.client_email || !sa?.private_key) throw new Error('GOOGLE_SERVICE_ACCOUNT needs client_email and private_key');
	return sa;
}

export async function googleAccessToken(json, scope = 'https://www.googleapis.com/auth/webmasters') {
	const sa = parseServiceAccount(json);
	const pem = sa.private_key.replace(/-----(BEGIN|END) PRIVATE KEY-----/g, '').replace(/\s+/g, '');
	const der = Uint8Array.from(atob(pem), (ch) => ch.charCodeAt(0));
	const key = await crypto.subtle.importKey('pkcs8', der, { name: 'RSASSA-PKCS1-v1_5', hash: 'SHA-256' }, false, ['sign']);
	const aud = sa.token_uri || TOKEN_URL;
	const now = Math.floor(Date.now() / 1000);
	const unsigned = `${b64urlText(JSON.stringify({ alg: 'RS256', typ: 'JWT' }))}.${b64urlText(JSON.stringify({ iss: sa.client_email, scope, aud, iat: now, exp: now + 3600 }))}`;
	const sig = await crypto.subtle.sign('RSASSA-PKCS1-v1_5', key, new TextEncoder().encode(unsigned));
	const res = await fetch(aud, {
		method: 'POST',
		headers: { 'content-type': 'application/x-www-form-urlencoded' },
		body: new URLSearchParams({ grant_type: 'urn:ietf:params:oauth:grant-type:jwt-bearer', assertion: `${unsigned}.${b64url(sig)}` })
	});
	const body = await res.json().catch(() => ({}));
	if (!res.ok || !body.access_token) throw new Error(`Google sign-in failed: ${body.error_description || body.error || res.status}`);
	return { token: body.access_token, email: sa.client_email };
}
