import type { Container } from '@human-synthesis/norns/server'
import type { TwoFactorRepo } from './repo'
import { newSecret, verifyTotp, otpauthUri, sealSecret, openSecret, sha256, recoveryCodes } from './totp.js'
import { qrSvg } from './qr.js'
import { policyFor } from '../shared/policy'
import { auth, audit } from '../../auth/server/public'
import { settings, envVar } from '../../settings/server/public'

// Errors are interface-text keys (i18n catalogue): the admin shows them in
// English, the members' account in the visitor's language.
export class TwoFactorService
	repo: TwoFactorRepo
	c: Container
	constructor(@repo: TwoFactorRepo, @c: Container)

	appSecret()
		envVar 'CMS_SECRET'

	async policy(user: any): Promise<string>
		values := await settings(@c).get 'twofactor'
		policyFor values, user

	async enabled(userId: number): Promise<boolean>
		!!(await @repo.get(userId))?.enabled_at

	enabledIds(ids: number[])
		@repo.enabledIds ids

	async mustEnrol(user: any): Promise<boolean>
		return false unless (await @policy(user)) === 'required'
		!(await @enabled(user.id))

	async status(user: any)
		row := await @repo.get user.id
		policy := await @policy user
		enabled := !!row?.enabled_at
		{
			enabled
			enabledAt: row?.enabled_at ?? null
			recoveryCodesLeft: enabled ? (JSON.parse(row?.recovery_codes || '[]') as string[]).length : 0
			policy
			mustEnrol: policy === 'required' && !enabled
		}

	async setupFor(user: any, secret: string)
		site := await settings(@c).get 'site'
		uri := otpauthUri secret, user.email, site.title || 'Norns CMS'
		{ secret, uri, qr: qrSvg(uri) }

	// Starts (or restarts) setup with a fresh secret; the old one, if any,
	// keeps nothing: a user who had it on must finish setup again.
	async begin(user: any)
		secret := newSecret()
		await @repo.start user.id, sealSecret(secret, @appSecret())
		@setupFor user, secret

	// The setup waiting for its first code, to show again after a wrong one.
	async pending(user: any)
		row := await @repo.get user.id
		return null unless row && !row.enabled_at
		secret := openSecret row.secret, @appSecret()
		secret ? await @setupFor(user, secret) : null

	async confirm(user: any, code: string)
		row := await @repo.get user.id
		secret := row && !row.enabled_at ? openSecret(row.secret, @appSecret()) : null
		return { ok: false, error: 'twofactor.restart' } unless secret
		return { ok: false, error: 'twofactor.wrongCode' } unless verifyTotp(secret, code)
		codes := recoveryCodes()
		await @repo.enable user.id, codes.map((c) => sha256(c))
		await audit(@c).log user, 'user.2fa_enabled', user.email
		{ ok: true, codes }

	async disable(user: any, password: string)
		return { ok: false, error: 'twofactor.wrongPassword', field: 'password' } unless await auth(@c).checkPassword(user.id, password)
		return { ok: false, error: 'twofactor.cannotDisable' } if (await @policy(user)) === 'required'
		await @repo.remove user.id
		await audit(@c).log user, 'user.2fa_disabled', user.email
		{ ok: true }

	// Staff remove someone else's two-factor (lost phone). Same owner rules
	// as any other change to that user.
	async reset(id: number, actor: any, meta: Record<string, any> = {})
		target := await auth(@c).getUser id
		return { ok: false, error: 'User not found' } unless target && target.status !== 'deleted'
		problem := await auth(@c).guard actor, id
		return { ok: false, error: problem } if problem
		await @repo.remove id
		await audit(@c).log actor, 'user.2fa_reset', target.email, meta
		{ ok: true }

	// The sign-in step: a TOTP code, or a recovery code (used up on use).
	async verify(user: any, code: string): Promise<boolean>
		row := await @repo.get user.id
		return false unless row?.enabled_at
		secret := openSecret row.secret, @appSecret()
		return true if secret && verifyTotp(secret, code)
		hashed := sha256 String(code).trim().toLowerCase()
		codes := JSON.parse(row.recovery_codes || '[]') as string[]
		return false unless codes.includes hashed
		await @repo.setCodes user.id, codes.filter((h) => h !== hashed)
		await audit(@c).log user, 'login.recovery_code', user.email, { remaining: codes.length - 1 }
		true
