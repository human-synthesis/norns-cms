import type { Container } from '@human-synthesis/norns/server'
import type { ContentType } from '../shared/types'

PREFIX := 'cms.type:'

// Content types are registered as `cms.type:<id>` bindings on the root
// container (see registerType in public.c). Reading them lazily here, rather
// than at module registration, keeps the registry independent of the order
// boot() runs feature modules in.
export class TypeRegistry
	root: Container
	constructor(scope: Container)
		node .= scope
		while node.parent
			node = node.parent
		@root = node

	all(): ContentType[]
		out := [] as ContentType[]
		for [token] of @root.bindings
			if token.startsWith(PREFIX)
				out.push @root.resolve(token) as ContentType
		out

	get(id: string): ContentType | undefined
		return undefined unless @root.bindings.has(PREFIX + id)
		@root.resolve(PREFIX + id) as ContentType
