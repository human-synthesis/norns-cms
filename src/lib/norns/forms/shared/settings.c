// Settings → Forms.
export formsSettings := {
	id: 'forms'
	label: 'Forms'
	icon: 'lucide:inbox'
	order: 45
	capability: 'forms.manage'
	description: 'Where the site shows a form outside pages, and how long submissions are kept.'
	fields: [
		{ name: 'footerForm', label: 'Form in the footer', type: 'select', optionsFrom: 'forms', help: 'Shown on every public page, e.g. the newsletter signup. Empty: none' }
		{ name: 'retentionDays', label: 'Keep submissions for (days)', type: 'number', default: 730, required: true, help: 'Older submissions are deleted by the nightly maintenance' }
	]
}

export formBlock := {
	id: 'form', label: 'Form', icon: 'lucide:mail', order: 12
	description: 'A contact form, the newsletter signup or any form from Forms'
	fields: [
		{ name: 'form', label: 'Form', type: 'select', optionsFrom: 'forms', required: true }
		{ name: 'title', label: 'Heading', type: 'text' }
		{ name: 'text', label: 'Text above the form', type: 'textarea' }
	]
}
