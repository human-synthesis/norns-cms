import type { BlockDef } from '../../content/shared/types'

// The general-purpose page blocks. Feature-specific blocks are registered by
// their features: articleList (articles), location (seo), cookieTable (consent).
export heroBlock := {
	id: 'hero', label: 'Hero', icon: 'lucide:panel-top', order: 1
	description: 'Large heading with text, an image and up to two buttons'
	fields: [
		{ name: 'title', label: 'Heading', type: 'text', required: true }
		{ name: 'text', label: 'Text', type: 'textarea' }
		{ name: 'image', label: 'Image', type: 'image' }
		{ name: 'primaryLabel', label: 'Primary button label', type: 'text' }
		{ name: 'primaryHref', label: 'Primary button link', type: 'url' }
		{ name: 'secondaryLabel', label: 'Secondary button label', type: 'text' }
		{ name: 'secondaryHref', label: 'Secondary button link', type: 'url' }
		{ name: 'align', label: 'Alignment', type: 'select', default: 'left', options: [{ value: 'left', label: 'Left' }, { value: 'center', label: 'Centered' }] }
	]
} as BlockDef

export markdownBlock := {
	id: 'markdown', label: 'Text', icon: 'lucide:pilcrow', order: 2
	description: 'Rich Markdown: headings, images, callouts, embeds, tables, code'
	fields: [
		{ name: 'body', label: 'Body', type: 'markdown' }
	]
} as BlockDef

export columnsBlock := {
	id: 'columns', label: 'Columns', icon: 'lucide:columns-3', order: 3
	description: 'Two or three columns of Markdown'
	fields: [
		{ name: 'title', label: 'Heading', type: 'text' }
		{ name: 'columns', label: 'Columns', type: 'repeater', fields: [
			{ name: 'title', label: 'Column heading', type: 'text' }
			{ name: 'body', label: 'Body', type: 'markdown' }
		] }
	]
} as BlockDef

export ctaBlock := {
	id: 'cta', label: 'Call to action', icon: 'lucide:megaphone', order: 4
	description: 'A short pitch with one button'
	fields: [
		{ name: 'title', label: 'Heading', type: 'text', required: true }
		{ name: 'text', label: 'Text', type: 'textarea' }
		{ name: 'buttonLabel', label: 'Button label', type: 'text' }
		{ name: 'buttonHref', label: 'Button link', type: 'url' }
		{ name: 'tone', label: 'Style', type: 'select', default: 'accent', options: [{ value: 'accent', label: 'Accent' }, { value: 'subtle', label: 'Subtle' }] }
	]
} as BlockDef

export imageBlock := {
	id: 'image', label: 'Image', icon: 'lucide:image', order: 5
	fields: [
		{ name: 'image', label: 'Image', type: 'image', required: true }
		{ name: 'caption', label: 'Caption', type: 'text' }
		{ name: 'wide', label: 'Full width', type: 'boolean' }
	]
} as BlockDef

export galleryBlock := {
	id: 'gallery', label: 'Gallery', icon: 'lucide:images', order: 6
	fields: [
		{ name: 'title', label: 'Heading', type: 'text' }
		{ name: 'images', label: 'Images', type: 'repeater', fields: [
			{ name: 'image', label: 'Image', type: 'image', required: true }
			{ name: 'caption', label: 'Caption', type: 'text' }
		] }
	]
} as BlockDef

export faqBlock := {
	id: 'faq', label: 'FAQ', icon: 'lucide:circle-help', order: 7
	description: 'Questions and answers; also published as FAQPage structured data'
	fields: [
		{ name: 'title', label: 'Heading', type: 'text', default: 'Frequently asked questions' }
		{ name: 'items', label: 'Questions', type: 'repeater', fields: [
			{ name: 'question', label: 'Question', type: 'text', required: true }
			{ name: 'answer', label: 'Answer', type: 'markdown' }
		] }
	]
} as BlockDef

export embedBlock := {
	id: 'embed', label: 'Video / embed', icon: 'lucide:youtube', order: 8
	description: 'YouTube, Vimeo or any https page; loads only after marketing consent or a click'
	fields: [
		{ name: 'url', label: 'URL', type: 'url', required: true, help: 'A YouTube or Vimeo link, or an https URL to embed' }
		{ name: 'title', label: 'Title', type: 'text' }
	]
} as BlockDef

export pageBlocks := [heroBlock, markdownBlock, columnsBlock, ctaBlock, imageBlock, galleryBlock, faqBlock, embedBlock]
