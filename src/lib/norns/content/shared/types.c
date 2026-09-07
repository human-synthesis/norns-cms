// Content type definitions are plain objects declared in code (see
// pages/shared/types.c) and registered at boot. Kept loosely typed on
// purpose: the shape is validated by compile.c, not by TypeScript.
//
// FieldDef: { name, label, type, required?, help?, placeholder?, options? }
//   type is one of: text | textarea | markdown | number | boolean |
//   datetime | select | tags
// ContentType: { id, label, labelPlural, icon?, basePath, listable?,
//   titleField?, fields: FieldDef[] }
//   basePath '' means entries resolve at /<slug>; '/posts' means
//   /posts/<slug>, and with listable: true, /posts lists them.
export type FieldDef = Record<string, any>
export type ContentType = Record<string, any>

export FIELD_TYPES := ['text', 'textarea', 'markdown', 'number', 'boolean', 'datetime', 'select', 'tags']
