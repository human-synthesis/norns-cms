import type { Container } from '@human-synthesis/norns/server'
import { TypeRegistry } from './registry'
import { EntriesRepo } from './repo'
import { ContentService } from './service'

// The registry is a root singleton (types never change after boot); repo and
// service are transient because `db` is resolved per request (D1 binding on
// Workers, SQLite otherwise — see auth/server/module.c).
export default (app: Container) =>
	app.single 'content.registry', (c: Container) => new TypeRegistry c
	app.bind 'content.repo', (c: Container) => new EntriesRepo c.resolve('db')
	app.bind 'content.service', (c: Container) => new ContentService c.resolve('content.registry'), c.resolve('content.repo')
