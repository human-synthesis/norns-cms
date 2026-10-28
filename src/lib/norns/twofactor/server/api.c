import type { Container } from '@human-synthesis/norns/server'
import { registerEndpoint, apiError, need } from '../../api/server/public'
import { twoFactor } from './public'
import { auth } from '../../auth/server/public'

// Setting up one's own two-factor needs the authenticator in hand, so it
// stays in the admin and the account area; the API reads status and resets.
export registerTwoFactorApi := (app: Container) =>
	registerEndpoint app, {
		method: 'GET', path: '/users/:id/2fa', capability: 'users.view', tag: 'Users'
		summary: 'Two-factor status of a user: on or off, recovery codes left, and what the policy asks of them'
		handler: async ({ c, params }) =>
			u := await auth(c).getUser Number(params.id)
			await twoFactor(c).status need(u, 'User not found')
	}
	registerEndpoint app, {
		method: 'POST', path: '/users/:id/2fa/reset', capability: 'users.manage', tag: 'Users'
		summary: 'Remove a user’s two-factor sign-in (lost device); they set it up again at next sign-in if the policy requires it'
		handler: async ({ c, params, user }) =>
			r := await twoFactor(c).reset Number(params.id), user, { via: 'api' }
			throw apiError((r.error === 'User not found' ? 404 : 400), r.error) unless r.ok
			{ reset: true }
	}
