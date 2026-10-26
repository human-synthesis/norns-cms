import type { Container } from '@human-synthesis/norns/server'

// Settings groups are `settings.group:<id>` bindings on the root container,
// contributed by any feature's module.c through registerSettings().
PREFIX := 'settings.group:'

export rootOf := (c: Container) =>
	node .= c
	while node.parent
		node = node.parent
	node

export class SettingsRegistry
	root: Container
	constructor(scope: Container)
		@root = rootOf scope

	all(): any[]
		out := [] as any[]
		for [token] of @root.bindings
			if token.startsWith(PREFIX)
				out.push @root.resolve(token)
		out.sort (a, b) => (a.order ?? 100) - (b.order ?? 100)

	get(id: string): any
		return undefined unless @root.bindings.has(PREFIX + id)
		@root.resolve PREFIX + id
