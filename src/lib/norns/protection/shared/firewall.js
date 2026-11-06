// Cloudflare WAF custom rules built from the blocked paths and the country
// filter. Pure: expressions in Cloudflare's Rules language, scoped to the
// site's own hostnames so other sites on the zone are untouched.
//
// `wildcard` compares case-insensitively and `*` matches anything,
// including `/` — the same meaning as the CMS's own patterns.

export const REF_PATHS = 'norns_cms_paths';
export const REF_COUNTRIES = 'norns_cms_countries';
export const isOurs = (rule) => typeof rule?.ref === 'string' && rule.ref.startsWith('norns_cms_');

/** Free plans accept 4096 characters per expression; stay below. */
export const MAX_EXPRESSION = 3800;

const str = (s) => `"${String(s).replace(/\\/g, '\\\\').replace(/"/g, '\\"')}"`;

export const hostClause = (hosts) => `http.host in {${[...new Set(hosts)].sort().map(str).join(' ')}}`;

/** Paths the team needs, left out of the country rule unless the admin is filtered too. */
export const TEAM_WILDCARDS = ['/admin*', '/login*', '/reset*', '/invite*', '/verify*', '/api/v1*'];

/**
 * Blocked-path rules: one or more (split to stay under the length limit).
 * → [{ ref, description, expression }]
 */
export function pathRules(patterns, hosts) {
	const list = [...new Set(patterns)].sort();
	if (!list.length || !hosts.length) return [];
	const host = hostClause(hosts);
	const room = MAX_EXPRESSION - host.length - 20;
	const chunks = [[]];
	let size = 0;
	for (const p of list) {
		const part = `http.request.uri.path wildcard ${str(p)}`;
		if (size + part.length + 4 > room && chunks.at(-1).length) {
			chunks.push([]);
			size = 0;
		}
		chunks.at(-1).push(part);
		size += part.length + 4;
	}
	return chunks.map((parts, i) => ({
		ref: chunks.length === 1 ? REF_PATHS : `${REF_PATHS}_${i + 1}`,
		description: `norns-cms: blocked paths${chunks.length > 1 ? ` (${i + 1}/${chunks.length})` : ''} — managed by the CMS, edit there`,
		expression: `${host} and (${parts.join(' or ')})`
	}));
}

/** The country rule, or null when the filter is off. */
export function countryRule(filter, hosts) {
	if (!filter || filter.mode === 'off' || !filter.countries.length || !hosts.length) return null;
	const set = `{${filter.countries.map(str).join(' ')}}`;
	const unknown = 'ip.src.country eq "XX"';
	let who = filter.mode === 'allow' ? `not ip.src.country in ${set}` : `ip.src.country in ${set}`;
	if (filter.mode === 'allow' && filter.unknown !== 'block') who = `(${who} and not ${unknown})`;
	if (filter.mode === 'block' && filter.unknown === 'block') who = `(${who} or ${unknown})`;
	const parts = [hostClause(hosts), who, 'not http.request.uri.path wildcard "/api/cron*"'];
	if (!filter.admin) parts.push(`not (${TEAM_WILDCARDS.map((w) => `http.request.uri.path wildcard ${str(w)}`).join(' or ')})`);
	return {
		ref: REF_COUNTRIES,
		description: `norns-cms: country filter (${filter.mode === 'allow' ? 'only' : 'all but'} ${filter.countries.length}) — managed by the CMS, edit there`,
		expression: parts.join(' and ')
	};
}

/** The zone names a host could belong to, longest first (www.a.example.co.uk → a.example.co.uk, example.co.uk, co.uk). */
export function zoneCandidates(host) {
	const parts = String(host ?? '').toLowerCase().split('.').filter(Boolean);
	const out = [];
	for (let i = 0; i < parts.length - 1; i++) out.push(parts.slice(i).join('.'));
	return out;
}
