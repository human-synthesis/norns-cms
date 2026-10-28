import type { Container } from '@human-synthesis/norns/server'
import { fail } from '@sveltejs/kit'
import type { TwoFactorService } from './service'

export { policyFor } from '../shared/policy'

svc := (c: Container) => c.resolve('twofactor.service') as TwoFactorService

// Two-factor sign-in (TOTP + recovery codes) for staff and members, with
// separate policies (Settings → Two-factor sign-in). auth asks it during
// sign-in through registerSecondFactor; routes use this facade for setup.
export twoFactor := (c: Container) => {
	status: (user: any) => svc(c).status(user)
	policy: (user: any) => svc(c).policy(user)
	enabled: (userId: number) => svc(c).enabled(userId)
	enabledIds: (ids: number[]) => svc(c).enabledIds(ids)
	begin: (user: any) => svc(c).begin(user)
	pending: (user: any) => svc(c).pending(user)
	confirm: (user: any, code: string) => svc(c).confirm(user, code)
	disable: (user: any, password: string) => svc(c).disable(user, password)
	reset: (id: number, actor: any, meta?: Record<string, any>) => svc(c).reset(id, actor, meta)
}

// The form actions behind ui/TwoFactorCard.n, shared by the admin profile
// and the members' account page. `t` turns an interface-text key into the
// page's language (English in the admin).
export twoFactorActions := (t: (event: any, container: any) => Promise<(key: string, vars?: any) => string>) => {
	twoFactorBegin:
		run: async ({ container, user }) =>
			{ setup: await twoFactor(container).begin(user) }
	twoFactorConfirm:
		run: async ({ container, event, user }) =>
			code := String((await event.request.formData()).get('code') ?? '')
			r := await twoFactor(container).confirm user, code
			unless r.ok
				tr := await t event, container
				return fail 400, { setup: await twoFactor(container).pending(user), errors: [{ path: [{ key: 'code' }], message: tr(r.error) }] }
			tr := await t event, container
			{ codes: r.codes, saved: tr('twofactor.enabled') }
	twoFactorDisable:
		run: async ({ container, event, user }) =>
			password := String((await event.request.formData()).get('password') ?? '')
			r := await twoFactor(container).disable user, password
			tr := await t event, container
			return fail 400, { errors: [{ path: [{ key: 'disablePassword' }], message: tr(r.error) }] } unless r.ok
			{ saved: tr('twofactor.disabled') }
}
