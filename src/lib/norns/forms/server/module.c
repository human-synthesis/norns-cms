import type { Container } from '@human-synthesis/norns/server'
import { FormsRepo } from './repo'
import { FormsService } from './service'
import { registerAdminNav, registerOptions, registerBlock } from '../../content/server/public'
import { registerCapabilities } from '../../auth/server/public'
import { registerSettings } from '../../settings/server/public'
import { formsSettings, formBlock } from '../shared/settings'
import { registerFormsApi } from './api'
import { registerFormSource } from '../../protection/server/public'

svc := (c: Container) => c.resolve('forms.service') as FormsService

export default (app: Container) =>
	app.bind 'forms.repo', (c: Container) => new FormsRepo c.resolve('db')
	app.bind 'forms.service', (c: Container) => new FormsService c.resolve('forms.repo'), c
	registerCapabilities app, [
		{ id: 'forms.view', label: 'Read form submissions', group: 'Forms' }
		{ id: 'forms.manage', label: 'Edit forms; archive and delete submissions', group: 'Forms' }
	]
	registerSettings app, formsSettings
	registerOptions app, 'forms', (c: Container) => svc(c).options()
	registerAdminNav app, { id: 'forms', label: 'Forms', icon: 'lucide:inbox', section: 'Marketing', order: 65, capability: 'forms.view' }
	registerAdminNav app, { id: 'forms-inbox', parent: 'forms', label: 'Submissions', href: '/admin/forms/submissions', order: 1 }
	registerAdminNav app, { id: 'forms-all', parent: 'forms', label: 'All forms', href: '/admin/forms', order: 2 }
	registerAdminNav app, { id: 'forms-settings', parent: 'forms', label: 'Settings', href: '/admin/settings/forms', order: 3, capability: 'forms.manage' }
	// A form on a page, in the page's language.
	registerBlock app, {
		...formBlock
		resolve: async (props: any, { c, entry }: any) => ({ form: props.form ? await svc(c).localized(String(props.form), entry?.locale ?? 'en') : null })
	}
	registerFormsApi app
	// Each form made in Forms is protected as forms:<slug>.
	registerFormSource app, 'forms', async (c: Container) =>
		(await svc(c).list()).map (f: any) => ({ id: `forms:${f.slug}`, label: f.name, group: 'Forms', path: `/forms/${f.slug}`, minMs: 2500 })
