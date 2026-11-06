// Country filter: the country list, presets and the pure decision. The
// country comes from Cloudflare (request.cf.country / CF-IPCountry): an ISO
// 3166-1 alpha-2 code, 'T1' for Tor, 'XX' or nothing when unknown.

export const COUNTRY_CODES = (
	'AD AE AF AG AI AL AM AO AQ AR AS AT AU AW AX AZ BA BB BD BE BF BG BH BI BJ BL BM BN BO BQ BR BS BT BV BW BY BZ ' +
	'CA CC CD CF CG CH CI CK CL CM CN CO CR CU CV CW CX CY CZ DE DJ DK DM DO DZ EC EE EG EH ER ES ET FI FJ FK FM FO FR ' +
	'GA GB GD GE GF GG GH GI GL GM GN GP GQ GR GS GT GU GW GY HK HM HN HR HT HU ID IE IL IM IN IO IQ IR IS IT JE JM JO JP ' +
	'KE KG KH KI KM KN KP KR KW KY KZ LA LB LC LI LK LR LS LT LU LV LY MA MC MD ME MF MG MH MK ML MM MN MO MP MQ MR MS MT ' +
	'MU MV MW MX MY MZ NA NC NE NF NG NI NL NO NP NR NU NZ OM PA PE PF PG PH PK PL PM PN PR PS PT PW PY QA RE RO RS RU RW ' +
	'SA SB SC SD SE SG SH SI SJ SK SL SM SN SO SR SS ST SV SX SY SZ TC TD TF TG TH TJ TK TL TM TN TO TR TT TV TW TZ UA UG ' +
	'UM US UY UZ VA VC VE VG VI VN VU WF WS XK YE YT ZA ZM ZW'
).split(' ');

export const TOR = 'T1';

const EU = 'AT BE BG HR CY CZ DK EE FI FR DE GR HU IE IT LV LT LU MT NL PL PT RO SK SI ES SE'.split(' ');
export const PRESETS = [
	{ id: 'eu', label: 'European Union', codes: EU },
	{ id: 'eea', label: 'EEA (EU + Iceland, Liechtenstein, Norway)', codes: [...EU, 'IS', 'LI', 'NO'] },
	{ id: 'europe', label: 'Europe', codes: [...EU, 'AD', 'AL', 'BA', 'BY', 'CH', 'FO', 'GB', 'GG', 'GI', 'IM', 'IS', 'JE', 'LI', 'MC', 'MD', 'ME', 'MK', 'NO', 'RS', 'RU', 'SM', 'UA', 'VA', 'XK', 'AX', 'SJ'] },
	{ id: 'na', label: 'North America', codes: ['US', 'CA', 'MX'] }
];

let names = null;
/** English country names (Intl), with Tor and Kosovo spelled out. */
export function countryName(code) {
	try {
		names ??= new Intl.DisplayNames(['en'], { type: 'region' });
		if (code === TOR) return 'Tor network';
		if (code === 'XK') return 'Kosovo';
		return names.of(code) ?? code;
	} catch {
		return code;
	}
}

export const countryOptions = () =>
	[...COUNTRY_CODES, TOR].map((code) => ({ value: code, label: countryName(code) })).sort((a, b) => a.label.localeCompare(b.label));

export const DEFAULT_FILTER = { mode: 'off', countries: [], unknown: 'allow', admin: false };

/** Paths the filter never touches: the Worker's own cron call. */
const ALWAYS = /^\/api\/cron(\/|$)/;
/** Team paths: filtered only when `admin` is on. */
export const TEAM = /^\/(admin|login|reset|invite|verify|api\/v1)(\/|$)/;

/** Clean stored / posted settings. */
export function cleanFilter(input = {}) {
	const known = new Set([...COUNTRY_CODES, TOR]);
	const list = Array.isArray(input.countries) ? input.countries : String(input.countries ?? '').split(/[\s,]+/);
	return {
		mode: ['off', 'allow', 'block'].includes(input.mode) ? input.mode : 'off',
		countries: [...new Set(list.map((c) => String(c).trim().toUpperCase()).filter((c) => known.has(c)))].sort(),
		unknown: input.unknown === 'block' ? 'block' : 'allow',
		admin: input.admin === true || input.admin === 'on' || input.admin === 'true' || input.admin === '1'
	};
}

/** Is a request from `country` for `path` let through? */
export function allowed(filter, country, path) {
	if (!filter || filter.mode === 'off') return true;
	if (ALWAYS.test(path)) return true;
	if (!filter.admin && TEAM.test(path)) return true;
	const code = String(country ?? '').toUpperCase();
	if (!code || code === 'XX') return filter.unknown !== 'block';
	const listed = filter.countries.includes(code);
	return filter.mode === 'allow' ? listed : !listed;
}
