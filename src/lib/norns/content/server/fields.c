import * as v from 'valibot'
import type { FieldDef } from '../shared/types'

// One valibot factory per field type. Inputs arrive as strings from
// formData() or as JSON values (blocks, autosave, settings patches), so
// every schema first normalises to a string with `str(f)` and coerces
// from there. Absent keys arrive as undefined; `optional(…, '')` as the
// FIRST step keeps a missing required field on our own message instead of
// valibot's generic "Invalid key".
//
// ctx: { compile(fields) → { schema }, block(id) → BlockDef | undefined }
// (needed by `blocks` and `repeater`, which validate nested values).

// `default` applies only when the key is absent (blocks JSON, API input,
// settings patches); forms always send their fields.
str := (f: FieldDef) => v.pipe(
	v.optional(v.unknown(), f.default ?? ''),
	v.transform((x: any) =>
		if x == null
			''
		else if Array.isArray(x)
			x.join(',')
		else if typeof x === 'object'
			JSON.stringify(x)
		else
			String(x)
	)
)

required := (f: FieldDef, base: any) =>
	if f.required
		v.pipe base, v.trim(), v.minLength(1, `${f.label} is required`)
	else
		v.pipe base, v.trim()

text := (f: FieldDef) =>
	s := required f, str(f)
	if f.max
		v.pipe s, v.maxLength(f.max, `${f.label} must be at most ${f.max} characters`)
	else
		s

email := (f: FieldDef) =>
	v.pipe required(f, str(f)), v.check((s) => s === '' || /^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(s), `${f.label} must be an email address`)

url := (f: FieldDef) =>
	v.pipe required(f, str(f)), v.check((s) => s === '' || s.startsWith('/') || /^https?:\/\//i.test(s), `${f.label} must start with https:// or /`)

number := (f: FieldDef) =>
	parsed := v.pipe(
		str(f),
		v.trim(),
		v.transform((s) => s === '' ? null : Number(s)),
		v.check((n) => n === null || Number.isFinite(n), `${f.label} must be a number`)
	)
	if f.required
		v.pipe parsed, v.check((n) => n !== null, `${f.label} is required`)
	else
		parsed

// Editors render a hidden "false" before each switch, so an unticked box
// still arrives and a ticked one arrives as ['false', 'on']: the last value
// wins.
boolean := (f: FieldDef) =>
	v.pipe v.optional(v.unknown(), f.default ?? false), v.transform((raw) =>
		x := Array.isArray(raw) ? raw[raw.length - 1] : raw
		x === 'on' || x === 'true' || x === true || x === '1' || x === 1
	)

select := (f: FieldDef) =>
	// Dynamic options (optionsFrom) are checked by their provider, not here.
	return required(f, str(f)) if f.optionsFrom
	allowed := (f.options ?? []).map((o) => String(o.value))
	if f.required
		v.pipe str(f), v.picklist(allowed, `${f.label} is required`)
	else
		v.pipe str(f), v.picklist([...allowed, ''], `${f.label}: pick one of the options`)

tags := (f: FieldDef) =>
	v.pipe str(f), v.transform((s) =>
		seen := new Set<string>()
		out := [] as string[]
		for t of s.split(',')
			tag := t.trim()
			if tag && !seen.has(tag.toLowerCase())
				seen.add tag.toLowerCase()
				out.push tag
		out
	)

lines := (f: FieldDef) =>
	v.pipe(
		v.optional(v.unknown(), f.default ?? ''),
		v.transform((x: any) => Array.isArray(x) ? x.join('\n') : String(x ?? '')),
		v.transform((s) => s.split(/\r?\n/).map((l) => l.trim()).filter(Boolean))
	)

// A single id (media, entry, term): '' → null.
id := (f: FieldDef) =>
	parsed := v.pipe(
		str(f),
		v.trim(),
		v.transform((s) => s === '' ? null : Number(s)),
		v.check((n) => n === null || (Number.isInteger(n) && n > 0), `${f.label} is not valid`)
	)
	if f.required
		v.pipe parsed, v.check((n) => n !== null, `${f.label} is required`)
	else
		parsed

// Many ids: an array (multi-select / JSON) or a comma list.
ids := (f: FieldDef) =>
	v.pipe(
		v.optional(v.unknown(), []),
		v.transform((x: any) =>
			list := Array.isArray(x) ? x : String(x ?? '').split(',')
			out := [] as number[]
			for item of list
				n := Number(String(item).trim())
				out.push n if Number.isInteger(n) && n > 0 && !out.includes(n)
			out
		),
		v.check((a) => !f.required || a.length > 0, `${f.label} is required`)
	)

parseJson := (x: any, fallback: any) =>
	return x if typeof x === 'object' && x !== null
	try
		JSON.parse(String(x || '') || JSON.stringify(fallback))
	catch
		undefined

json := (f: FieldDef) =>
	v.pipe(
		v.optional(v.unknown(), null),
		v.transform((x) => parseJson(x, null)),
		v.check((x) => x !== undefined, `${f.label} must be valid JSON`)
	)

uid := => Math.random().toString(36).slice(2, 10)

// Nested values are validated leniently: each item runs through the
// compiled sub-schema and invalid items keep their raw values plus a
// collected issue, so one bad block cannot silently erase the others.
coerceItem := (fields: FieldDef[], item: any, ctx: any) =>
	{ schema } := ctx.compile fields
	r := v.safeParse schema, item ?? {}
	r.success ? { ok: true, value: r.output } : { ok: false, issues: r.issues }

repeater := (f: FieldDef, ctx: any) =>
	v.pipe(
		v.optional(v.unknown(), []),
		v.transform((x) => parseJson(x, [])),
		v.check((x) => Array.isArray(x), `${f.label} must be a list`),
		v.rawTransform(({ dataset, addIssue, NEVER }) =>
			out := [] as any[]
			for item, i of dataset.value as any[]
				r := coerceItem f.fields ?? [], item, ctx
				if r.ok
					out.push r.value
				else
					addIssue { message: `${f.label} ${i + 1}: ${r.issues[0]?.message}` }
					return NEVER
			out
		)
	)

blocks := (f: FieldDef, ctx: any) =>
	v.pipe(
		v.optional(v.unknown(), []),
		v.transform((x) => parseJson(x, [])),
		v.check((x) => Array.isArray(x), `${f.label} must be a list of blocks`),
		v.rawTransform(({ dataset, addIssue, NEVER }) =>
			out := [] as any[]
			for raw, i of dataset.value as any[]
				def := ctx.block?.(raw?.type)
				unless def
					addIssue { message: `Block ${i + 1}: unknown block type "${raw?.type}"` }
					return NEVER
				r := coerceItem def.fields, raw.props ?? {}, ctx
				unless r.ok
					addIssue { message: `${def.label} block (${i + 1}): ${r.issues[0]?.message}` }
					return NEVER
				out.push { id: String(raw.id || uid()), type: def.id, props: r.value }
			out
		)
	)

export fieldSchemas := {
	text
	textarea: text
	markdown: text
	code: text
	datetime: text
	email
	url
	number
	boolean
	select
	tags
	lines
	image: id
	reference: id
	term: id
	references: ids
	users: ids
	json
	repeater
	blocks
}
