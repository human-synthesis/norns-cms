import type { Container } from '@human-synthesis/norns/server'
import { TypeRegistry } from './registry'
import { EntriesRepo } from './repo'
import { ContentService } from './service'
import { registerCapabilities } from '../../auth/server/public'

// The registry is a root singleton (types never change after boot); repo and
// service are transient because `db` is resolved per request (D1 binding on
// Workers, SQLite otherwise — see auth/server/module.c). The service gets
// the resolving container so hooks and resolvers run in the request scope.
export default (app: Container) =>
	app.single 'content.registry', (c: Container) => new TypeRegistry c
	app.bind 'content.repo', (c: Container) => new EntriesRepo c.resolve('db')
	app.bind 'content.service', (c: Container) => new ContentService c.resolve('content.registry'), c.resolve('content.repo'), c
	registerCapabilities app, [
		{ id: 'content.view', label: 'View content in the admin', group: 'Content' }
		{ id: 'content.create', label: 'Create entries', group: 'Content' }
		{ id: 'content.edit_own', label: 'Edit own unpublished entries', group: 'Content' }
		{ id: 'content.edit_any', label: 'Edit any entry', group: 'Content' }
		{ id: 'content.publish', label: 'Publish, schedule and unpublish', group: 'Content' }
		{ id: 'content.delete', label: 'Delete entries', group: 'Content' }
		{ id: 'content.members', label: 'Read members-only content', group: 'Members' }
	]
