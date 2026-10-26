// Content type definitions are plain objects declared in code (see
// pages/shared/types.c) and registered at boot. Kept loosely typed on
// purpose: the shape is validated by compile.c, not by TypeScript.
//
// FieldDef: { name, label, type, required?, help?, placeholder?, options?,
//   default?, tab?, to?, taxonomy?, optionsFrom?, fields? (repeater) }
//   type is one of FIELD_TYPES below. `tab` puts the field on an editor tab
//   (default 'content'); `optionsFrom` names an option provider registered
//   with registerOptions (select / reference / term / users fields).
// ContentType: { id, label, labelPlural, icon?, basePath, listable?,
//   titleField?, summaryField?, seo? (default true), schemaType?,
//   tabs?: [{ id, label }], fields: FieldDef[] }
//   basePath '' means entries resolve at /<slug>; '/articles' means
//   /articles/<slug>, and with listable: true, /articles lists them.
// BlockDef: { id, label, icon?, description?, fields: FieldDef[] }
export type FieldDef = Record<string, any>
export type ContentType = Record<string, any>
export type BlockDef = Record<string, any>

export FIELD_TYPES := [
	'text', 'textarea', 'markdown', 'number', 'boolean', 'datetime', 'select', 'tags',
	'email', 'url', 'lines', 'code', 'image', 'reference', 'references', 'users',
	'term', 'blocks', 'repeater', 'json'
]

export STATUSES := ['draft', 'in_review', 'published']

export VISIBILITIES := [
	{ value: 'public', label: 'Public' }
	{ value: 'members', label: 'Members only' }
]

export DEFAULT_TABS := [
	{ id: 'content', label: 'Content' }
]

// Status as shown to people: a published entry with a future date is
// "scheduled" until then.
export displayStatus := (entry: any, now = Date.now()) =>
	return 'new' unless entry
	if entry.status === 'published' && entry.published_at && entry.published_at > now
		'scheduled'
	else
		entry.status
