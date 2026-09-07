import * as v from 'valibot'
import type { ContentType } from '../shared/types'
import { fieldSchemas } from './fields'

// A content type definition → the valibot schema its editor and API use.
// Called once per type and cached by the service.
export compileType := (ct: ContentType) =>
	shape := {} as Record<string, any>
	for f of ct.fields
		factory := (fieldSchemas as any)[f.type]
		unless factory
			throw new Error(`content: unknown field type "${f.type}" on ${ct.id}.${f.name}`)
		shape[f.name] = factory f
	schema := v.object shape
	{ schema, fields: ct.fields, titleField: ct.titleField ?? 'title' }
