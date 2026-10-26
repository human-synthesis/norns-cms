import type { ContentType } from '../../content/shared/types'

// Articles resolve at /articles/<slug>; /articles lists them (featured
// first), with archives per category, tag and author.
export articleType := {
	id: 'article'
	label: 'Article'
	labelPlural: 'Articles'
	icon: 'lucide:newspaper'
	basePath: '/articles'
	listable: true
	order: 2
	schemaType: 'BlogPosting'
	summaryField: 'summary'
	teaserFields: ['category', 'tags', 'authors', 'readingTime']
	tabs: [
		{ id: 'content', label: 'Content' }
		{ id: 'details', label: 'Details' }
	]
	fields: [
		{ name: 'title', label: 'Title', type: 'text', required: true }
		{ name: 'summary', label: 'Summary', type: 'textarea', help: 'The answer in two or three sentences. Shown as the TL;DR, in listings, search results and llms.txt' }
		{ name: 'cover', label: 'Cover image', type: 'image' }
		{ name: 'body', label: 'Body', type: 'markdown' }
		{ name: 'category', label: 'Category', type: 'term', taxonomy: 'category', tab: 'details' }
		{ name: 'tags', label: 'Tags', type: 'tags', tab: 'details', help: 'Comma separated; new tags are created on save' }
		{ name: 'authors', label: 'Authors', type: 'users', tab: 'details', help: 'Defaults to you' }
		{ name: 'featured', label: 'Featured', type: 'boolean', tab: 'details', help: 'Pin to the top of the article list' }
		{ name: 'keyFacts', label: 'Key facts', type: 'lines', tab: 'details', help: 'One fact per line; shown in a box and used as the structured-data abstract' }
		{ name: 'sources', label: 'Sources', type: 'lines', tab: 'details', help: 'One per line: Title | https://link' }
	]
} as ContentType

export articleListBlock := {
	id: 'articleList', label: 'Article list', icon: 'lucide:list', order: 9
	description: 'Latest, featured, or one category or tag'
	fields: [
		{ name: 'title', label: 'Heading', type: 'text', default: 'Latest articles' }
		{ name: 'source', label: 'Show', type: 'select', default: 'latest', required: true, options: [
			{ value: 'latest', label: 'Latest' }
			{ value: 'featured', label: 'Featured' }
			{ value: 'category', label: 'One category' }
			{ value: 'tag', label: 'One tag' }
		] }
		{ name: 'category', label: 'Category', type: 'term', taxonomy: 'category' }
		{ name: 'tag', label: 'Tag', type: 'text', help: 'Tag name, when showing one tag' }
		{ name: 'count', label: 'How many', type: 'number', default: 3 }
		{ name: 'layout', label: 'Layout', type: 'select', default: 'grid', options: [{ value: 'grid', label: 'Cards' }, { value: 'list', label: 'List' }] }
		{ name: 'showMore', label: 'Link to all articles', type: 'boolean', default: true }
	]
}

// "Title | https://url" lines → { title, url }.
export parseSources := (lines: string[]) =>
	(lines ?? []).map (line) =>
		[title, url] := line.split('|').map((s) => s.trim())
		url ? { title: title || url, url } : (/^https?:\/\//.test(title) ? { title, url: title } : { title, url: '' })

export readingMinutes := (words: number) => Math.max 1, Math.round(words / 220)
