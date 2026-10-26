import type { Container } from '@human-synthesis/norns/server'
import type { ContentType, BlockDef, FieldDef } from '../shared/types'

// Everything the content engine is extended with lives as prefixed
// bindings on the root container, so any feature's module.c can add to it
// and registration order never matters:
//
//   cms.type:<id>              content types          registerType
//   cms.fieldgroup:<id>        fields added to types  registerFieldGroup
//   cms.block:<id>             page blocks            registerBlock
//   cms.hook:<event>:<id>      lifecycle hooks        registerHook
//   cms.options:<key>          dynamic select options registerOptions
//   cms.nav:<id>               admin navigation       registerAdminNav

export rootOf := (c: Container) =>
	node .= c
	while node.parent
		node = node.parent
	node

export class TypeRegistry
	root: Container
	constructor(scope: Container)
		@root = rootOf scope

	prefixed(prefix: string): any[]
		out := [] as any[]
		for [token] of @root.bindings
			if token.startsWith(prefix)
				out.push @root.resolve(token)
		out

	all(): ContentType[]
		@prefixed('cms.type:').sort (a, b) => (a.order ?? 100) - (b.order ?? 100)

	get(id: string): ContentType | undefined
		return undefined unless @root.bindings.has('cms.type:' + id)
		@root.resolve 'cms.type:' + id

	fieldGroups(): any[]
		@prefixed 'cms.fieldgroup:'

	// The type's own fields plus every field group that applies to it, each
	// tagged with the tab it renders on.
	fieldsFor(ct: ContentType): FieldDef[]
		own := ct.fields.map (f: FieldDef) => ({ tab: 'content', ...f })
		extra := [] as FieldDef[]
		for g of @fieldGroups()
			if !g.appliesTo || g.appliesTo(ct)
				for f of g.fields
					extra.push { ...f, tab: f.tab ?? g.tab ?? g.id }
		[...own, ...extra]

	tabsFor(ct: ContentType): { id: string, label: string }[]
		tabs := [...(ct.tabs ?? [{ id: 'content', label: 'Content' }])]
		for g of @fieldGroups()
			if (!g.appliesTo || g.appliesTo(ct)) && !tabs.some((t) => t.id === (g.tab ?? g.id))
				tabs.push { id: g.tab ?? g.id, label: g.tabLabel ?? g.label }
		tabs

	blocks(): BlockDef[]
		@prefixed('cms.block:').sort (a, b) => (a.order ?? 100) - (b.order ?? 100)

	block(id: string): BlockDef | undefined
		return undefined unless id && @root.bindings.has('cms.block:' + id)
		@root.resolve 'cms.block:' + id

	hooks(event: string): any[]
		@prefixed `cms.hook:${event}:`

	options(key: string): any
		return undefined unless @root.bindings.has('cms.options:' + key)
		@root.resolve 'cms.options:' + key

	nav(): any[]
		@prefixed('cms.nav:').sort (a, b) => (a.order ?? 100) - (b.order ?? 100)
