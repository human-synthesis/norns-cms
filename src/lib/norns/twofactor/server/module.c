import type { Container } from '@human-synthesis/norns/server'
import { TwoFactorRepo } from './repo'
import { TwoFactorService } from './service'
import { registerSecondFactor } from '../../auth/server/public'
import { registerSettings } from '../../settings/server/public'
import { registerAdminNav } from '../../content/server/public'
import { twoFactorSettings } from '../shared/policy'
import { registerTwoFactorApi } from './api'

svc := (c: Container) => c.resolve('twofactor.service') as TwoFactorService

export default (app: Container) =>
	app.bind 'twofactor.repo', (c: Container) => new TwoFactorRepo c.resolve('db')
	app.bind 'twofactor.service', (c: Container) => new TwoFactorService c.resolve('twofactor.repo'), c
	registerSecondFactor app, {
		enabled: (c: Container, userId: number) => svc(c).enabled(userId)
		verify: (c: Container, user: any, code: string) => svc(c).verify(user, code)
		mustEnrol: (c: Container, user: any) => svc(c).mustEnrol(user)
	}
	registerSettings app, twoFactorSettings
	registerAdminNav app, { id: 'settings-twofactor', parent: 'settings', label: 'Two-factor sign-in', href: '/admin/settings/twofactor', order: 3 }
	registerTwoFactorApi app
