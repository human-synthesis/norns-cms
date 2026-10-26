import * as v from 'valibot'
import type { FieldDef } from '../shared/types'
import { fieldSchemas } from './fields'

// Field definitions → a valibot object schema. Used for content types
// (fields + field groups), settings groups, blocks and repeater items.
//
// ctx.block(id) looks up block definitions for `blocks` fields; it is
// threaded through so nested repeaters and blocks compile with the same
// lookups.
export compileFields := (fields: FieldDef[], ctx: any = {}) =>
	inner := { ...ctx }
	inner.compile = (sub: FieldDef[]) => compileFields sub, inner
	shape := {} as Record<string, any>
	for f of fields
		factory := (fieldSchemas as any)[f.type]
		unless factory
			throw new Error(`content: unknown field type "${f.type}" on field "${f.name}"`)
		shape[f.name] = factory f, inner
	{ schema: v.object(shape), fields }
