// Interface-text lookup, shared by server and client. `strings` is the
// merged map for one language (built by the i18n service: overrides, own
// catalogue, fallback language's, English).
import { CATALOGS, en } from './catalog.js';

export function fill(text, vars = {}) {
	return String(text).replace(/\{(\w+)\}/g, (m, k) => (vars[k] == null ? m : String(vars[k])));
}

export function translate(strings, key, vars) {
	return fill(strings?.[key] ?? en[key] ?? key, vars);
}

/** Built-in texts for a language through its fallback chain. */
export function builtIn(chain) {
	const out = {};
	for (const code of [...chain].reverse()) {
		Object.assign(out, CATALOGS[code] ?? CATALOGS[String(code).split('-')[0]] ?? {});
	}
	return { ...en, ...out };
}
