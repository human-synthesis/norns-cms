import type { Container } from '@human-synthesis/norns/server'
import type { SettingsService } from './service'

export { envVar, binding, currentEvent, siteOrigin } from './env.js'
export { securityHandle, buildCsp } from './security.js'
export { runMaintenance } from './maintenance'

svc := (c: Container) => c.resolve('settings.service') as SettingsService

// Settings facade. Groups are declared in code (registerSettings) with the
// same field definitions as content types; values live in one row per group.
export settings := (c: Container) => {
	groups: () => svc(c).groups()
	group: (id: string) => svc(c).group(id)
	get: (id: string) => svc(c).get(id)
	save: (id: string, raw: Record<string, any>, userId: number | null) => svc(c).save(id, raw, userId)
	patch: (id: string, values: Record<string, any>, userId: number | null) => svc(c).patch(id, values, userId)
}

// group: { id, label, icon?, order?, capability?, description?, fields: FieldDef[] }
export registerSettings := (app: Container, group: any) =>
	app.single `settings.group:${group.id}`, => group
