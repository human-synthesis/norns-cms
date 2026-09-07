import type { Container } from '@human-synthesis/norns/server'
import { TypeRegistry } from './registry'
import { EntriesRepo } from './repo'
import { ContentService } from './service'

export default (app: Container) =>
	app.single 'content.registry', (c: Container) => new TypeRegistry c
	app.single 'content.repo', (c: Container) => new EntriesRepo c.resolve('db')
	app.single 'content.service', (c: Container) => new ContentService c.resolve('content.registry'), c.resolve('content.repo')
