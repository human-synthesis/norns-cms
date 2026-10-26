// Consent categories, the cookie format, regional rules and the settings
// group. Pure, so the banner (client) and the handle (server) agree.

export CATEGORIES := ['preferences', 'analytics', 'marketing']
BITS := { preferences: 1, analytics: 2, marketing: 4 } as Record<string, number>

export COOKIE := 'cms_consent'

// EU + EEA. UK and Switzerland are their own regions so they can differ.
EEA := ['AT', 'BE', 'BG', 'HR', 'CY', 'CZ', 'DK', 'EE', 'FI', 'FR', 'DE', 'GR', 'HU', 'IE', 'IT', 'LV', 'LT', 'LU', 'MT', 'NL', 'PL', 'PT', 'RO', 'SK', 'SI', 'ES', 'SE', 'IS', 'LI', 'NO']

export regionOf := (country: string) =>
	cc := String(country ?? '').toUpperCase()
	return 'eea' if EEA.includes(cc)
	return 'uk' if cc === 'GB'
	return 'ch' if cc === 'CH'
	'other'

export modeFor := (region: string, cfg: Record<string, any>) =>
	key := ({ eea: 'modeEea', uk: 'modeUk', ch: 'modeCh', other: 'modeOther' } as Record<string, string>)[region] ?? 'modeOther'
	String(cfg[key] || 'opt-in')

export type Choice = { preferences: boolean, analytics: boolean, marketing: boolean }

// Cookie value: v1.<consentId>.<policyVersion>.<bits>.<timestamp>
export encodeConsent := (id: string, version: number, choice: Choice, at = Date.now()) =>
	picked := choice as Record<string, boolean>
	bits .= 0
	for cat of CATEGORIES
		if picked[cat]
			bits = bits | BITS[cat]
	`v1.${id}.${version}.${bits}.${at}`

export decodeConsent := (value: string | undefined | null) =>
	return null unless value
	parts := String(value).split('.')
	return null unless parts.length === 5 && parts[0] === 'v1' && /^[a-f0-9]{16,64}$/.test(parts[1])
	bits := Number(parts[3])
	return null unless Number.isInteger(bits)
	choice := {} as Record<string, boolean>
	for cat of CATEGORIES
		choice[cat] = (bits & BITS[cat]) !== 0
	{ id: parts[1], version: Number(parts[2]), choice: choice as Choice, at: Number(parts[4]) }

// What applies before (or without) a choice, by regional mode.
export defaultsFor := (mode: string, gpc: boolean): Choice =>
	if mode === 'opt-out' || mode === 'notice'
		{ preferences: true, analytics: true, marketing: !gpc }
	else
		{ preferences: false, analytics: false, marketing: false }

MODES := [
	{ value: 'opt-in', label: 'Opt-in — nothing optional until the visitor agrees (GDPR, ePrivacy)' }
	{ value: 'opt-out', label: 'Opt-out — on by default, the banner allows refusing' }
	{ value: 'notice', label: 'Notice — information only' }
]

export consentSettings := {
	id: 'consent'
	label: 'Cookie consent'
	icon: 'lucide:cookie'
	order: 30
	capability: 'consent.manage'
	description: 'Banner wording, categories, regional rules and retention. Scripts are managed under Consent → Scripts.'
	fields: [
		{ name: 'enabled', label: 'Show the cookie banner', type: 'boolean', default: true, help: 'Turn off only if the site sets no optional cookies at all' }
		{ name: 'bannerTitle', label: 'Banner title', type: 'text', default: 'We value your privacy' }
		{ name: 'bannerText', label: 'Banner text', type: 'textarea', default: 'We use cookies to run this site and, with your permission, to understand how it is used and to show embedded media. You can change your choice at any time from "Cookie settings" at the bottom of every page.' }
		{ name: 'policyUrl', label: 'Privacy policy link', type: 'url', default: '/privacy' }
		{ name: 'policyLabel', label: 'Privacy policy link text', type: 'text', default: 'Privacy policy' }
		{ name: 'policyVersion', label: 'Policy version', type: 'number', default: 1, required: true, help: 'Raising it asks every visitor again (e.g. after adding a marketing script)' }
		{ name: 'cookieDays', label: 'Remember the choice for (days)', type: 'number', default: 180, required: true, help: 'At most 395 days (13 months)' }
		{ name: 'retentionMonths', label: 'Keep consent records for (months)', type: 'number', default: 24, required: true }
		{ name: 'modeEea', label: 'EU / EEA', type: 'select', default: 'opt-in', required: true, options: MODES }
		{ name: 'modeUk', label: 'United Kingdom', type: 'select', default: 'opt-in', required: true, options: MODES }
		{ name: 'modeCh', label: 'Switzerland', type: 'select', default: 'opt-in', required: true, options: MODES }
		{ name: 'modeOther', label: 'Everywhere else', type: 'select', default: 'opt-in', required: true, options: MODES }
		{ name: 'necessaryText', label: 'Necessary — description', type: 'textarea', default: 'Required for the site to work: signing in, security, and remembering your cookie choice. Always on.' }
		{ name: 'preferencesLabel', label: 'Preferences — name', type: 'text', default: 'Preferences' }
		{ name: 'preferencesText', label: 'Preferences — description', type: 'textarea', default: 'Remember settings such as theme or language.' }
		{ name: 'analyticsLabel', label: 'Analytics — name', type: 'text', default: 'Analytics' }
		{ name: 'analyticsText', label: 'Analytics — description', type: 'textarea', default: 'Help us understand which pages are useful, with aggregated statistics.' }
		{ name: 'marketingLabel', label: 'Marketing — name', type: 'text', default: 'Marketing & embedded media' }
		{ name: 'marketingText', label: 'Marketing — description', type: 'textarea', default: 'Measure campaigns and load content from other sites such as YouTube videos, which may track you.' }
	]
}

export cookieTableBlock := {
	id: 'cookieTable', label: 'Cookie table', icon: 'lucide:cookie', order: 11
	description: 'The cookie declaration, generated from the enabled scripts'
	fields: [
		{ name: 'title', label: 'Heading', type: 'text', default: 'Cookies we use' }
		{ name: 'showButton', label: 'Show a "Cookie settings" button', type: 'boolean', default: true }
	]
}
