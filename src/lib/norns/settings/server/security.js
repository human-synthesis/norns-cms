// Security headers and the per-request script nonce.
//
// Every <script> in a rendered HTML page gets the request's nonce through
// transformPageChunk, including SvelteKit's own bootstrap and the consent
// loader. CSP is `script-src 'nonce-…' 'strict-dynamic'`, so scripts loaded
// by a nonced script (GA, GTM, …) are allowed and nothing else is. The
// consent loader copies the nonce onto scripts it activates later.
//
// Mode comes from the `security` settings group: enforce | report-only | off.
// In dev the header is skipped (Vite injects un-nonced inline scripts).
import { randomBytes } from 'node:crypto';

const NONCE_RE = /<script(?![^>]*\snonce=)/g;

export function buildCsp(nonce) {
	return [
		"default-src 'self'",
		`script-src 'nonce-${nonce}' 'strict-dynamic' https: 'unsafe-inline'`,
		"style-src 'self' 'unsafe-inline' https:",
		"img-src 'self' data: blob: https:",
		"font-src 'self' data: https:",
		"connect-src 'self' https:",
		"frame-src 'self' https:",
		"media-src 'self' https:",
		"object-src 'none'",
		"base-uri 'self'",
		"form-action 'self'",
		"frame-ancestors 'self'"
	].join('; ');
}

/**
 * @param {{ dev?: boolean, mode?: (event: any) => Promise<string> }} opts
 */
export function securityHandle(opts = {}) {
	return async ({ event, resolve }) => {
		const nonce = randomBytes(16).toString('base64');
		event.locals.nonce = nonce;
		const response = await resolve(event, {
			transformPageChunk: ({ html }) => html.replace(NONCE_RE, `<script nonce="${nonce}"`)
		});
		const mode = opts.dev ? 'off' : ((await opts.mode?.(event).catch(() => 'enforce')) ?? 'enforce');
		try {
			response.headers.set('X-Content-Type-Options', 'nosniff');
			response.headers.set('Referrer-Policy', 'strict-origin-when-cross-origin');
			response.headers.set('Permissions-Policy', 'camera=(), microphone=(), geolocation=(), interest-cohort=()');
			const path = event.url.pathname;
			if (path.startsWith('/admin') || path.startsWith('/account') || path.startsWith('/login')) {
				response.headers.set('X-Frame-Options', 'SAMEORIGIN');
				response.headers.set('Cache-Control', 'private, no-store');
			}
			const type = response.headers.get('content-type') ?? '';
			if (type.startsWith('text/html') && mode !== 'off') {
				const name = mode === 'report-only' ? 'Content-Security-Policy-Report-Only' : 'Content-Security-Policy';
				response.headers.set(name, buildCsp(nonce));
			}
		} catch {
			// Immutable headers (a passed-through upstream Response); leave as is.
		}
		return response;
	};
}
