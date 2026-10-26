// Environment access that works on Workers and under Bun.
//
// On Workers, vars and secrets live on `event.platform.env` (wrangler.toml
// [vars], `wrangler secret put`); under Bun / Node and in tests they are
// process.env. Feature code must not import `$env/*` (bun test cannot
// resolve it), so everything goes through envVar().
import { getScope } from '@human-synthesis/norns/server';

/** @param {string} key */
export function envVar(key) {
	const env = getScope()?.event?.platform?.env;
	if (env && env[key] != null && env[key] !== '') return String(env[key]);
	if (typeof process !== 'undefined' && process.env && process.env[key]) return process.env[key];
	return undefined;
}

/** A Cloudflare binding (R2 bucket, D1, ...) from the current request, if any. */
export function binding(key) {
	return getScope()?.event?.platform?.env?.[key];
}

/** The current request event (or undefined outside a request). */
export function currentEvent() {
	return getScope()?.event;
}

/** Absolute origin for links in emails, feeds and sitemaps. CMS_PUBLIC_URL wins. */
export function siteOrigin() {
	const configured = envVar('CMS_PUBLIC_URL');
	if (configured) return configured.replace(/\/+$/, '');
	const event = getScope()?.event;
	if (event?.url) return event.url.origin;
	return 'http://localhost:5173';
}
