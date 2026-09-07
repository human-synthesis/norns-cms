import type { ContentType } from '../../content/shared/types'

// Posts resolve at /posts/<slug>; /posts lists the published ones.
export postType := {
	id: 'post',
	label: 'Post',
	labelPlural: 'Posts',
	icon: 'lucide:newspaper',
	basePath: '/posts',
	listable: true,
	fields: [
		{ name: 'title', label: 'Title', type: 'text', required: true },
		{ name: 'excerpt', label: 'Excerpt', type: 'textarea', help: 'Shown in listings' },
		{ name: 'body', label: 'Body', type: 'markdown', help: 'Markdown' },
		{ name: 'tags', label: 'Tags', type: 'tags' },
	],
} as ContentType
