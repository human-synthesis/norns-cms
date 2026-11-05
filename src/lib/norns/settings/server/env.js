// Environment access that works on Workers and under Bun.
//
// On Workers, vars, secrets and bindings live on the `env` of the
// `cloudflare:workers` module (wrangler.toml [vars], `wrangler secret put`,
// D1 / R2 bindings). SvelteKit 3 no longer puts them on `event.platform`;
// adapter-cloudflare emulates the module in `bun run dev`. Under Bun / Node
// and in tests the module does not exist and values come from process.env.
// Feature code must not import `$env/*` or `cloudflare:workers` directly
// (bun test cannot resolve them), so everything goes through envVar().
import { getScope } from '@human-synthesis/norns/server';

/** @type {Record<string, any> | undefined} */
let workerEnv;
try {
	workerEnv = (await import('cloudflare:workers')).env;
} catch {
	workerEnv = undefined;
}

/** The Workers env (vars, secrets, bindings), or undefined outside Workers. */
function platformEnv() {
	if (workerEnv) {
		try {
			// The dev emulation throws while prerendering; treat that as "none".
			if (Reflect.ownKeys(workerEnv).length) return workerEnv;
		} catch {
			/* no env in this context */
		}
	}
	// Tests and older adapters may still hand one in on the event.
	return getScope()?.event?.platform?.env;
}

/** @param {string} key */
export function envVar(key) {
	const env = platformEnv();
	if (env && env[key] != null && env[key] !== '') return String(env[key]);
	if (typeof process !== 'undefined' && process.env && process.env[key]) return process.env[key];
	return undefined;
}

/** A Cloudflare binding (R2 bucket, D1, ...), if any. */
export function binding(key) {
	return platformEnv()?.[key];
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
