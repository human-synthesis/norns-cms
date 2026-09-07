import type { ContentType } from '../../content/shared/types'

// Pages resolve at /<slug>. A page with showInNav appears in the site header.
export pageType := {
	id: 'page',
	label: 'Page',
	labelPlural: 'Pages',
	icon: 'lucide:file-text',
	basePath: '',
	listable: false,
	fields: [
		{ name: 'title', label: 'Title', type: 'text', required: true },
		{ name: 'body', label: 'Body', type: 'markdown', help: 'Markdown' },
		{ name: 'showInNav', label: 'Show in navigation', type: 'boolean', help: 'Link this page from the site header' },
	],
} as ContentType
