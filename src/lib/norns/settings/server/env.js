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
/** @type {((p: Promise<any>) => void) | undefined} */
let workerWaitUntil;
try {
	const workers = await import('cloudflare:workers');
	workerEnv = workers.env;
	workerWaitUntil = workers.waitUntil;
} catch {
	workerEnv = undefined;
}
// adapter-cloudflare's dev emulation makes waitUntil() a no-op, so in dev
// background work runs inline instead of being dropped.
// Written out in full: Vite only replaces the literal import.meta.env.DEV.
// Bun defines import.meta.env too (DEV is undefined there).
const DEV = import.meta.env.DEV === true;

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

/**
 * Run work after the response on Workers (waitUntil from cloudflare:workers;
 * SvelteKit 3 no longer passes the execution context on event.platform).
 * Returns null where there is none (dev, Bun, tests): await the work instead.
 * @returns {((p: Promise<any>) => void) | null}
 */
export function backgroundTask() {
	if (workerWaitUntil && !DEV) return (p) => workerWaitUntil?.(p);
	const platform = getScope()?.event?.platform;
	const ctx = platform?.context ?? platform?.ctx;
	return ctx?.waitUntil ? (p) => ctx.waitUntil(p) : null;
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
