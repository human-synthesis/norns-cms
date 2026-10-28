// Settings groups owned by the settings feature itself. Other features
// register theirs from their own module.c (seo, consent).
export siteSettings := {
	id: 'site'
	label: 'Site'
	icon: 'lucide:globe'
	order: 1
	description: 'Name, home page and who may create an account.'
	fields: [
		{ name: 'title', label: 'Site title', type: 'text', required: true, default: 'Norns CMS', translatable: true }
		{ name: 'tagline', label: 'Tagline', type: 'text', default: '', translatable: true }
		{ name: 'homePage', label: 'Home page', type: 'reference', to: 'page', help: 'A page to show at /. Leave empty for the latest articles.' }
		{ name: 'footerText', label: 'Footer text', type: 'text', default: '', translatable: true }
		{ name: 'registration', label: 'Member registration', type: 'select', default: 'off', required: true, options: [
			{ value: 'off', label: 'Off — no client sign-up' }
			{ value: 'invite', label: 'Invite only — staff create client accounts' }
			{ value: 'open', label: 'Open — anyone can register' }
		] }
		{ name: 'imageResizing', label: 'Cloudflare Image Resizing', type: 'boolean', default: false, help: 'Serve resized images through /cdn-cgi/image (the zone must have it enabled)' }
	]
}

export securitySettings := {
	id: 'security'
	label: 'Security'
	icon: 'lucide:shield'
	order: 90
	description: 'Sessions, audit retention and the Content-Security-Policy. Two-factor sign-in has its own settings.'
	fields: [
		{ name: 'staffSessionDays', label: 'Staff session length (days)', type: 'number', default: 7, required: true }
		{ name: 'memberSessionDays', label: 'Member session length (days)', type: 'number', default: 30, required: true }
		{ name: 'csp', label: 'Content-Security-Policy', type: 'select', default: 'enforce', required: true, options: [
			{ value: 'enforce', label: 'Enforce' }
			{ value: 'report-only', label: 'Report only' }
			{ value: 'off', label: 'Off' }
		] }
		{ name: 'auditRetentionDays', label: 'Audit log retention (days)', type: 'number', default: 365, required: true }
	]
}
