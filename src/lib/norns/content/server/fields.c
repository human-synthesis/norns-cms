import * as v from 'valibot'
import type { FieldDef } from '../shared/types'

// One valibot factory per field type. Inputs arrive as strings from
// formData(), so every schema coerces from string first.
//
// A key that is absent from the form (unchecked checkbox, empty tag list,
// missing required field) arrives as undefined. valibot's object() reports
// that as a generic "Invalid key" unless the entry schema accepts undefined,
// and optional(schema, default) runs the default through `schema` — so the
// pattern here is optional(string(), '') as the FIRST pipe step, followed by
// the real checks. That way a missing required field fails minLength with
// our message, and defaults like [] or null come out of a transform.

optionalString := v.optional v.string(), ''

text := (f: FieldDef) =>
	if f.required
		v.pipe optionalString, v.trim(), v.minLength(1, `${f.label} is required`)
	else
		v.pipe optionalString, v.trim()

number := (f: FieldDef) =>
	parsed := v.pipe(
		optionalString,
		v.trim(),
		v.transform((s) => s === '' ? null : Number(s)),
		v.check((n) => n === null || Number.isFinite(n), `${f.label} must be a number`)
	)
	if f.required
		v.pipe parsed, v.check((n) => n !== null, `${f.label} is required`)
	else
		parsed

boolean := (_f: FieldDef) =>
	v.pipe v.optional(v.unknown(), false), v.transform((x) => x === 'on' || x === 'true' || x === true)

select := (f: FieldDef) =>
	allowed := (f.options ?? []).map((o) => String(o.value))
	if f.required
		v.pipe optionalString, v.picklist(allowed, `${f.label} is required`)
	else
		v.pipe optionalString, v.picklist([...allowed, ''])

tags := (_f: FieldDef) =>
	v.pipe optionalString, v.transform((s) => s.split(',').map((t) => t.trim()).filter(Boolean))

export fieldSchemas := {
	text,
	textarea: text,
	markdown: text,
	datetime: text,
	number,
	boolean,
	select,
	tags,
}
