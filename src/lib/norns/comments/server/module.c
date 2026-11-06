import type { Container } from '@human-synthesis/norns/server'
import { CommentsRepo } from './repo'
import { CommentsService } from './service'
import { registerFieldGroup, registerHook, registerAdminNav, registerOptions } from '../../content/server/public'
import { registerCapabilities } from '../../auth/server/public'
import { registerSettings } from '../../settings/server/public'
import { registerForm } from '../../protection/server/public'
import { commentsSettings, commentsFieldGroup } from '../shared/settings'
import { registerCommentsApi } from './api'

svc := (c: Container) => c.resolve('comments.service') as CommentsService

export default (app: Container) =>
	app.bind 'comments.repo', (c: Container) => new CommentsRepo c.resolve('db')
	app.bind 'comments.service', (c: Container) => new CommentsService c.resolve('comments.repo'), c

	registerCapabilities app, [
		{ id: 'comments.moderate', label: 'Approve, edit, answer and delete comments; comment settings', group: 'Comments' }
	]
	registerSettings app, commentsSettings
	registerFieldGroup app, commentsFieldGroup
	registerOptions app, 'comments:types', async (c: Container) => svc(c).typeOptions()
	registerAdminNav app, { id: 'comments', label: 'Comments', icon: 'lucide:message-square', section: 'Content', order: 40, capability: 'comments.moderate' }
	registerAdminNav app, { id: 'comments-moderate', parent: 'comments', label: 'Moderation', href: '/admin/comments', order: 1 }
	registerAdminNav app, { id: 'comments-settings', parent: 'comments', label: 'Settings', href: '/admin/settings/comments', order: 2 }
	registerForm app, { id: 'comments.post', label: 'Comment', group: 'Comments', path: '/comments', minMs: 3000 }

	// Deleting an entry deletes its comments.
	registerHook app, 'delete', 'comments.entry', ({ c, entry }) =>
		svc(c).entryRemoved entry.id

	registerCommentsApi app
