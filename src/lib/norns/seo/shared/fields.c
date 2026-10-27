// SEO fields added to every content type (field group), the site-wide SEO
// and GEO settings, and the editor checks (pure, shared by the editor's live
// panel and the site-wide report).

export AI_BOTS := [
	{ id: 'GPTBot', label: 'OpenAI GPTBot (training)' }
	{ id: 'OAI-SearchBot', label: 'OpenAI search (ChatGPT search)' }
	{ id: 'ChatGPT-User', label: 'ChatGPT browsing on behalf of a user' }
	{ id: 'ClaudeBot', label: 'Anthropic ClaudeBot (training)' }
	{ id: 'Claude-SearchBot', label: 'Anthropic search' }
	{ id: 'Claude-User', label: 'Claude fetching on behalf of a user' }
	{ id: 'PerplexityBot', label: 'Perplexity search' }
	{ id: 'Google-Extended', label: 'Google Gemini training (not Search)' }
	{ id: 'Applebot-Extended', label: 'Apple Intelligence training' }
	{ id: 'CCBot', label: 'Common Crawl' }
	{ id: 'Bytespider', label: 'ByteDance' }
	{ id: 'meta-externalagent', label: 'Meta AI' }
]

export botField := (id: string) => `allow_${id.replace(/[^a-zA-Z0-9]/g, '_')}`

export seoFieldGroup := {
	id: 'seo'
	label: 'SEO'
	tab: 'seo'
	appliesTo: (ct: any) => ct.seo !== false
	fields: [
		{ name: 'seoTitle', label: 'Meta title', type: 'text', help: 'Defaults to the title. Aim for 30–60 characters' }
		{ name: 'seoDescription', label: 'Meta description', type: 'textarea', help: 'Defaults to the summary. Aim for 70–160 characters' }
		{ name: 'seoImage', label: 'Social image', type: 'image', help: 'Defaults to the cover image, then the site default' }
		{ name: 'seoCanonical', label: 'Canonical URL', type: 'url', help: 'Only when this content lives at another URL first' }
		{ name: 'seoSchema', label: 'Structured data type', type: 'select', default: '', options: [
			{ value: '', label: 'Automatic' }
			{ value: 'WebPage', label: 'Web page' }
			{ value: 'AboutPage', label: 'About page' }
			{ value: 'ContactPage', label: 'Contact page' }
			{ value: 'Article', label: 'Article' }
			{ value: 'BlogPosting', label: 'Blog post' }
			{ value: 'NewsArticle', label: 'News article' }
			{ value: 'HowTo', label: 'How-to' }
		] }
		{ name: 'seoNoindex', label: 'Hide from search engines', type: 'boolean', help: 'Adds noindex and leaves the entry out of the sitemap' }
		{ name: 'seoNofollow', label: 'Do not follow links', type: 'boolean', help: 'Adds nofollow' }
		{ name: 'seoNoLlms', label: 'Leave out of llms.txt and Markdown twins', type: 'boolean' }
	]
}

export seoSettings := {
	id: 'seo'
	label: 'SEO & GEO'
	icon: 'lucide:search'
	order: 20
	capability: 'seo.manage'
	description: 'Search defaults, structured data about the organisation, AI crawlers, llms.txt and local business details.'
	fields: [
		{ name: 'titleTemplate', label: 'Title template', type: 'text', default: '%s · %site', help: '%s is the page title, %site the site title', translatable: true }
		{ name: 'defaultDescription', label: 'Default description', type: 'textarea', translatable: true }
		{ name: 'defaultImage', label: 'Default social image', type: 'image' }
		{ name: 'locale', label: 'Locale', type: 'text', default: 'en_US', help: 'Open Graph locale, e.g. en_GB, de_DE' }
		{ name: 'twitter', label: 'X / Twitter handle', type: 'text', placeholder: '@example' }
		{ name: 'orgType', label: 'Organisation type', type: 'select', default: 'Organization', options: [
			{ value: 'Organization', label: 'Organisation' }
			{ value: 'Person', label: 'Person' }
			{ value: 'LocalBusiness', label: 'Local business' }
		] }
		{ name: 'orgName', label: 'Organisation name', type: 'text', help: 'Defaults to the site title', translatable: true }
		{ name: 'orgLogo', label: 'Logo', type: 'image' }
		{ name: 'orgSameAs', label: 'Profiles', type: 'lines', help: 'One URL per line (LinkedIn, GitHub, Wikipedia …); used as sameAs' }
		{ name: 'googleVerification', label: 'Google Search Console token', type: 'text' }
		{ name: 'bingVerification', label: 'Bing Webmaster token', type: 'text' }
		{ name: 'robotsExtra', label: 'Extra robots.txt rules', type: 'code', help: 'Appended as is' }
		{ name: 'llmsSummary', label: 'llms.txt summary', type: 'textarea', help: 'What the site is about, for AI assistants. Defaults to the tagline', translatable: true }
		{ name: 'llmsFull', label: 'Publish llms-full.txt', type: 'boolean', default: true, help: 'All public content as one Markdown file' }
		...AI_BOTS.map((b) => ({ name: botField(b.id), label: b.label, type: 'boolean', default: true, help: `Allow ${b.id} in robots.txt` }))
		{ name: 'bizType', label: 'Business type', type: 'text', default: 'LocalBusiness', help: 'schema.org type, e.g. Restaurant, Dentist, ProfessionalService' }
		{ name: 'bizStreet', label: 'Street address', type: 'text' }
		{ name: 'bizCity', label: 'City', type: 'text' }
		{ name: 'bizRegion', label: 'Region / state', type: 'text' }
		{ name: 'bizPostal', label: 'Postal code', type: 'text' }
		{ name: 'bizCountry', label: 'Country code', type: 'text', placeholder: 'DE' }
		{ name: 'bizLat', label: 'Latitude', type: 'number' }
		{ name: 'bizLng', label: 'Longitude', type: 'number' }
		{ name: 'bizPhone', label: 'Phone', type: 'text' }
		{ name: 'bizEmail', label: 'Public email', type: 'email' }
		{ name: 'bizHours', label: 'Opening hours', type: 'lines', help: 'One per line, schema.org style: Mo-Fr 09:00-17:00' }
		{ name: 'bizPriceRange', label: 'Price range', type: 'text', placeholder: '€€' }
		{ name: 'bizArea', label: 'Service area', type: 'text', help: 'City or region served' }
	]
}

export locationBlock := {
	id: 'location', label: 'Location', icon: 'lucide:map-pin', order: 10
	description: 'Address, opening hours and map link from SEO & GEO settings'
	fields: [
		{ name: 'title', label: 'Heading', type: 'text', default: 'Visit us' }
		{ name: 'showMap', label: 'Show map link', type: 'boolean', default: true }
	]
}

// Live checks for the editor's SEO tab and the site report. `values` are
// the raw form values; `ctx` { siteTitle, template, body } adds context.
export seoChecks := (values: Record<string, any>, ctx: Record<string, any> = {}) =>
	issues := [] as { level: string, message: string }[]
	title := String(values.seoTitle || values.title || '').trim()
	full := String(ctx.template || '%s').replace('%s', title).replace('%site', ctx.siteTitle || '')
	desc := String(values.seoDescription || values.summary || values.excerpt || '').trim()
	if !title
		issues.push { level: 'error', message: 'Add a title' }
	else if full.length > 65
		issues.push { level: 'warning', message: `The title shows as ${full.length} characters; search engines cut it near 60` }
	else if title.length < 15
		issues.push { level: 'info', message: 'The title is short; a few more words help people choose it' }
	if !desc
		issues.push { level: 'warning', message: 'Add a summary or meta description so search results do not show a random snippet' }
	else if desc.length > 165
		issues.push { level: 'warning', message: `The description is ${desc.length} characters; it will be cut near 160` }
	else if desc.length < 50
		issues.push { level: 'info', message: 'The description is short; aim for 70–160 characters' }
	body := String(ctx.body ?? values.body ?? '')
	if body
		if /^#\s/m.test(body)
			issues.push { level: 'warning', message: 'The body has a level-1 heading; the title is already the page heading, start sections at ##' }
		images := body.match(/!\[\s*\]\(/g)
		if images
			issues.push { level: 'warning', message: `${images.length} image${images.length === 1 ? ' has' : 's have'} no alt text` }
		levels := [...body.matchAll(/^(#{2,6})\s/gm)].map((m) => m[1].length)
		for l, i of levels
			if i > 0 && l > levels[i - 1] + 1
				issues.push { level: 'info', message: 'A heading level is skipped (e.g. ## then ####)' }
				break
	if values.seoNoindex === 'on' || values.seoNoindex === true
		issues.push { level: 'info', message: 'Hidden from search engines (noindex)' }
	slug := String(values.slug ?? '')
	if slug.length > 75
		issues.push { level: 'info', message: 'The URL is long; shorter URLs are easier to share' }
	issues
