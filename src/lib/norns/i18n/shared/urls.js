// Locale URLs: building the base of a (localisation, language) pair and
// resolving a request's host + path back to one. Pure functions over the
// configuration so they are unit-tested and usable anywhere.
//
// cfg = {
//   primaryHost: 'site.com', protocol: 'https:',
//   languages: { [code]: { code, name, native, dir, fallback, paths } },
//   markets:   [{ id, code, name, mode, prefix, host, languages: [{ code, segment }],
//                 defaultLanguage, prefixDefault, countries, contentFallback, isPrimary }]
// }

const clean = (s) => String(s ?? '').replace(/^\/+|\/+$/g, '');

export const segmentOf = (market, code) => market.languages.find((l) => l.code === code)?.segment || code.toLowerCase();

export const hostOf = (cfg, market) => (market.mode === 'path' || !market.host ? cfg.primaryHost : market.host);

/** Path prefix ('' or '/ch/de') of a language inside a localisation. */
export function basePath(market, code) {
	const parts = [];
	if (market.mode === 'path' && clean(market.prefix)) parts.push(clean(market.prefix));
	if (code !== market.defaultLanguage || market.prefixDefault) parts.push(segmentOf(market, code));
	return parts.length ? '/' + parts.join('/') : '';
}

/** { origin, path } where a language of a localisation lives. */
export function baseFor(cfg, market, code) {
	return { origin: `${cfg.protocol ?? 'https:'}//${hostOf(cfg, market)}`, path: basePath(market, code) };
}

export const offers = (market, code) => market.languages.some((l) => l.code === code);

/** Accept-Language negotiation among codes; exact, then base language. */
export function negotiate(header, codes, fallback) {
	const wanted = String(header ?? '')
		.split(',')
		.map((part) => {
			const [tag, ...params] = part.trim().split(';');
			const q = params.find((p) => p.trim().startsWith('q='));
			return { tag: tag.trim().toLowerCase(), q: q ? Number(q.split('=')[1]) || 0 : 1 };
		})
		.filter((x) => x.tag && x.tag !== '*' && x.q > 0)
		.sort((a, b) => b.q - a.q);
	const lower = codes.map((c) => c.toLowerCase());
	for (const { tag } of wanted) {
		const i = lower.indexOf(tag);
		if (i !== -1) return codes[i];
	}
	for (const { tag } of wanted) {
		const base = tag.split('-')[0];
		const i = lower.findIndex((c) => c === base || c.split('-')[0] === base);
		if (i !== -1) return codes[i];
	}
	return fallback;
}

/**
 * Host + path → { market, language, base, rest } or { redirect, status }.
 * `rest` is the path after the locale base, always starting with '/'.
 */
export function resolveLocale(cfg, host, pathname, acceptLanguage = '') {
	const enabled = cfg.markets.filter((m) => m.enabled !== false);
	const markets = enabled.length ? enabled : cfg.markets;
	const primary = markets.find((m) => m.isPrimary) ?? markets[0];
	const segs = String(pathname).split('/').filter(Boolean);
	const h = String(host ?? '').toLowerCase().split(':')[0];

	let market = markets.find((m) => m.mode !== 'path' && m.host && m.host.toLowerCase() === h);
	let consumed = 0;
	if (!market) {
		const onPrimary = markets.filter((m) => m.mode === 'path' || !m.host);
		market = onPrimary.find((m) => clean(m.prefix) && segs[0] === clean(m.prefix));
		if (market) consumed = 1;
		else market = onPrimary.find((m) => !clean(m.prefix)) ?? primary;
	}
	if (!market) return null;

	const next = segs[consumed];
	const lang = market.languages.find((l) => (l.segment || l.code.toLowerCase()) === next);
	if (lang) {
		const rest = '/' + segs.slice(consumed + 1).join('/');
		return { market, language: lang.code, base: basePath(market, lang.code), rest };
	}
	const restSegs = segs.slice(consumed);
	if (!market.prefixDefault) {
		return { market, language: market.defaultLanguage, base: basePath(market, market.defaultLanguage), rest: '/' + restSegs.join('/') };
	}
	// Every language is prefixed: send the visitor to one.
	const codes = market.languages.map((l) => l.code);
	const root = restSegs.length === 0;
	const code = root ? negotiate(acceptLanguage, codes, market.defaultLanguage) : market.defaultLanguage;
	const to = basePath(market, code) + (restSegs.length ? '/' + restSegs.join('/') : '/');
	return { redirect: to, status: root ? 302 : 301, market, language: code };
}

/** hreflang codes for a language in a localisation: de-CH per targeted country, else the language. */
export function hreflangs(market, code) {
	const countries = market.countries ?? [];
	if (!countries.length || code.includes('-')) return [code];
	return countries.map((cc) => `${code}-${String(cc).toUpperCase()}`);
}
