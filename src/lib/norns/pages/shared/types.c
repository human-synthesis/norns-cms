import type { ContentType } from '../../content/shared/types'

// Pages resolve at /<slug>. A page is either one Markdown body or an ordered
// list of blocks (hero, columns, article lists, FAQ …). Pages flagged
// showInNav appear in the site header, ordered by navOrder.
export pageType := {
	id: 'page'
	label: 'Page'
	labelPlural: 'Pages'
	icon: 'lucide:file-text'
	basePath: ''
	listable: false
	order: 1
	schemaType: 'WebPage'
	tabs: [
		{ id: 'content', label: 'Content' }
		{ id: 'settings', label: 'Page settings' }
	]
	fields: [
		{ name: 'title', label: 'Title', type: 'text', required: true }
		{ name: 'layout', label: 'Layout', type: 'select', required: true, default: 'markdown', options: [
			{ value: 'markdown', label: 'Markdown — one rich text body' }
			{ value: 'blocks', label: 'Blocks — sections you arrange' }
		] }
		{ name: 'body', label: 'Body', type: 'markdown', showIf: { field: 'layout', equals: 'markdown' } }
		{ name: 'blocks', label: 'Blocks', type: 'blocks', showIf: { field: 'layout', equals: 'blocks' } }
		{ name: 'summary', label: 'Summary', type: 'textarea', tab: 'settings', help: 'One or two sentences; used for search results, social cards and llms.txt' }
		{ name: 'showInNav', label: 'Show in navigation', type: 'boolean', tab: 'settings', help: 'Link this page from the site header' }
		{ name: 'navLabel', label: 'Navigation label', type: 'text', tab: 'settings', help: 'Defaults to the title' }
		{ name: 'navOrder', label: 'Navigation order', type: 'number', tab: 'settings', help: 'Lower numbers come first' }
		{ name: 'hideTitle', label: 'Hide the page title', type: 'boolean', tab: 'settings', help: 'For pages that open with a hero block' }
	]
} as ContentType
