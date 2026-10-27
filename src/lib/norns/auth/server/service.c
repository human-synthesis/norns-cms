import { randomBytes } from 'node:crypto'
import type { Container } from '@human-synthesis/norns/server'
import type { UsersRepo, User, UserRow, UserQuery } from './repo'
import { hashPassword, verifyPassword } from './password.js'
import { passwordProblem } from './policy.js'
import { newSecret, verifyTotp, otpauthUri, sealSecret, openSecret, sha256, recoveryCodes } from './totp.js'
import { capMatches } from '../shared/can'
import { settings, envVar, siteOrigin } from '../../settings/server/public'
import { mail } from '../../mail/server/public'
import { tFor } from '../../i18n/server/public'
import { getScope } from '@human-synthesis/norns/server'

DAY := 24 * 60 * 60 * 1000
LOGIN_WINDOW := 15 * 60 * 1000
MAX_PER_EMAIL := 5
MAX_PER_IP := 20
// Compared against for unknown emails so timing does not reveal accounts.
// Built on first use: Workers forbid random values in global scope.
dummyHash .= ''
dummy := =>
	dummyHash ||= hashPassword(randomBytes(8).toString('hex'))
	dummyHash

export type SessionUser = User & { capabilities: string[], sessionId: string, needs2fa: boolean }

export type LoginResult = { ok: boolean, session?: { id: string, maxAge: number }, user?: SessionUser, error?: string, mfa?: string }

slugifyName := (s: string) =>
	s.toLowerCase().normalize('NFKD').replace(/[̀-ͯ]/g, '').replace(/[^a-z0-9]+/g, '-').replace(/^-+|-+$/g, '')

publicUser := (row: any): User =>
	{ password_hash, totp_secret, recovery_codes, ...rest } := row
	rest as User

export class AuthService
	repo: UsersRepo
	c: Container
	constructor(@repo: UsersRepo, @c: Container)

	appSecret()
		envVar 'CMS_SECRET'

	async security()
		await settings(@c).get 'security'

	// --- capabilities --------------------------------------------------------

	declaredCapabilities(): any[]
		node .= @c as any
		while node.parent
			node = node.parent
		out := [] as any[]
		for [token] of node.bindings
			if token.startsWith('auth.cap:')
				out.push node.resolve(token)
		out

	async capabilitiesFor(roleId: string): Promise<string[]>
		role := await @repo.role roleId
		role?.capabilities ?? []

	// --- sessions ----------------------------------------------------------

	async sessionDays(kind: string): Promise<number>
		sec := await @security()
		Number(kind === 'staff' ? sec.staffSessionDays : sec.memberSessionDays) || 7

	async startSession(row: UserRow | User, userAgent = '')
		id := randomBytes(32).toString('hex')
		days := await @sessionDays row.kind
		await @repo.createSession id, row.id, Date.now() + days * DAY, userAgent
		await @repo.update row.id, { last_login_at: Date.now() }
		{ id, maxAge: days * 24 * 60 * 60 }

	// Resolve a session cookie to a user with capabilities. Sessions slide:
	// seen again after an hour, the expiry moves forward.
	async userForSession(sessionId: string): Promise<SessionUser | undefined>
		row := await @repo.sessionUser sessionId
		return undefined unless row
		if row.status !== 'active'
			await @repo.deleteSession sessionId
			return undefined
		if Date.now() - (row.last_seen_at ?? 0) > 60 * 60 * 1000
			await @repo.touchSession sessionId, Date.now() + (await @sessionDays(row.kind)) * DAY
		{ session_id, expires_at, last_seen_at, ...user } := row
		caps := await @capabilitiesFor user.role
		sec := await @security()
		needs2fa := !!sec.require2fa && user.kind === 'staff' && ['owner', 'admin'].includes(user.role) && !user.totp_enabled
		{ ...user, capabilities: caps, sessionId: session_id, needs2fa } as SessionUser

	async logout(sessionId: string): Promise<void>
		await @repo.deleteSession sessionId
		return

	async sessions(userId: number)
		@repo.sessions userId

	async revokeSession(userId: number, sessionId: string)
		await @repo.deleteUserSession userId, sessionId
		return

	async revokeAll(userId: number, exceptId: string | null = null)
		await @repo.deleteSessions userId, exceptId
		return

	// --- rate limiting -----------------------------------------------------

	key(scope: string, value: string)
		sha256 `${scope}:${value.toLowerCase()}`

	// Generic limiter for other features (consent API, registration …).
	async rateLimit(scope: string, value: string, max: number, windowMs: number): Promise<boolean>
		(await @repo.hit(@key(scope, value), windowMs)) <= max

	// --- sign-in -----------------------------------------------------------

	async login(email: string, password: string, meta: { ip?: string, userAgent?: string } = {}): Promise<LoginResult>
		addr := email.trim().toLowerCase()
		emailKey := @key 'login:email', addr
		ipKey := @key 'login:ip', meta.ip ?? 'unknown'
		if (await @repo.peekHits(emailKey, LOGIN_WINDOW)) >= MAX_PER_EMAIL || (await @repo.peekHits(ipKey, LOGIN_WINDOW)) >= MAX_PER_IP
			await @audit null, addr, 'login.rate_limited', addr, {}
			return { ok: false, error: 'rate' }
		row := await @repo.byEmail addr
		// Hash even for unknown emails so timing does not reveal accounts.
		valid := verifyPassword password, row?.password_hash ?? dummy()
		if !row || !valid || row.status !== 'active'
			await @repo.hit emailKey, LOGIN_WINDOW
			await @repo.hit ipKey, LOGIN_WINDOW
			await @audit row?.id ?? null, addr, 'login.failed', addr, { reason: !row ? 'unknown' : (!valid ? 'password' : row.status) }
			return { ok: false, error: 'invalid' }
		await @repo.clearHits emailKey
		if row.kind === 'client' && !row.email_verified_at
			return { ok: false, error: 'unverified' }
		if row.totp_enabled
			raw := randomBytes(24).toString('hex')
			await @repo.createToken sha256(raw), row.id, 'mfa', Date.now() + 5 * 60 * 1000
			return { ok: false, mfa: raw }
		session := await @startSession row, meta.userAgent ?? ''
		await @audit row.id, row.email, 'login', row.email, {}
		user := (await @userForSession session.id) as SessionUser
		{ ok: true, session, user }

	// Second step: a TOTP code or one of the recovery codes.
	async loginMfa(mfaToken: string, code: string, meta: { ip?: string, userAgent?: string } = {}): Promise<LoginResult>
		ipKey := @key 'mfa:ip', meta.ip ?? 'unknown'
		if (await @repo.hit(ipKey, LOGIN_WINDOW)) > MAX_PER_IP
			return { ok: false, error: 'rate' }
		tok := await @repo.token sha256(mfaToken), 'mfa'
		return { ok: false, error: 'invalid' } unless tok
		row := await @repo.byId tok.user_id
		return { ok: false, error: 'invalid' } unless row && row.status === 'active'
		ok .= false
		secret := openSecret row.totp_secret, @appSecret()
		if secret && verifyTotp(secret, code)
			ok = true
		else
			hashed := sha256 String(code).trim().toLowerCase()
			codes := JSON.parse(row.recovery_codes || '[]') as string[]
			if codes.includes hashed
				ok = true
				await @repo.update row.id, { recovery_codes: JSON.stringify(codes.filter((h) => h !== hashed)) }
				await @audit row.id, row.email, 'login.recovery_code', row.email, { remaining: codes.length - 1 }
		unless ok
			await @audit row.id, row.email, 'login.failed', row.email, { reason: '2fa' }
			return { ok: false, mfa: mfaToken }
		await @repo.useToken tok.id
		session := await @startSession row, meta.userAgent ?? ''
		await @audit row.id, row.email, 'login', row.email, { mfa: true }
		user := (await @userForSession session.id) as SessionUser
		{ ok: true, session, user }

	// --- users -------------------------------------------------------------

	async uniqueUserSlug(name: string, excludeId = 0)
		base := slugifyName(name) || 'author'
		candidate .= base
		n .= 2
		while await @repo.slugTaken(candidate, excludeId)
			candidate = `${base}-${n}`
			n++
		candidate

	async createUser(email: string, name: string, password: string, role = 'editor', opts: { kind?: string, status?: string, verified?: boolean } = {}): Promise<number>
		addr := email.trim().toLowerCase()
		r := await @repo.role role
		kind := opts.kind ?? r?.kind ?? 'staff'
		slug := kind === 'staff' ? await @uniqueUserSlug(name || addr.split('@')[0]) : null
		await @repo.create { email: addr, name: name.trim(), passwordHash: hashPassword(password), role, kind, status: opts.status ?? 'active', verified: opts.verified ?? true, slug }

	async hasUsers(): Promise<boolean>
		(await @repo.count()) > 0

	listUsers(q: UserQuery = {})
		@repo.list q

	countUsers(q: UserQuery = {})
		@repo.countWhere q

	async getUser(id: number): Promise<User | undefined>
		row := await @repo.byId id
		row ? publicUser(row) : undefined

	async userByEmail(email: string): Promise<User | undefined>
		row := await @repo.byEmail email.trim().toLowerCase()
		row ? publicUser(row) : undefined

	// Public author data for bylines and author pages.
	async profiles(ids: number[])
		rows := await @repo.byIds ids
		rows.filter((u) => u.kind === 'staff').map (u) => ({ id: u.id, name: u.name || u.email.split('@')[0], slug: u.slug, bio: u.bio, avatar: u.avatar_media_id })

	async authorBySlug(slug: string)
		u := await @repo.bySlug slug
		u ? { id: u.id, name: u.name || u.email.split('@')[0], slug: u.slug, bio: u.bio, avatar: u.avatar_media_id } : undefined

	async staffOptions()
		rows := await @repo.list { kind: 'staff', status: 'active', limit: 500 }
		rows.map (u) => ({ value: u.id, label: u.name || u.email })

	// Owner rules: only owners touch owners or hand out the owner role; the
	// last active owner cannot be demoted, suspended or deleted; nobody
	// changes their own role or status (no accidental lockout).
	async guard(actor: any, target: User, next: { role?: string, status?: string }): Promise<string | null>
		actorIsOwner := actor?.role === 'owner'
		if (target.role === 'owner' || next.role === 'owner') && !actorIsOwner
			return 'Only an owner can change owners'
		if actor?.id === target.id && ((next.role && next.role !== target.role) || (next.status && next.status !== target.status))
			return 'You cannot change your own role or status'
		if target.role === 'owner' && target.status === 'active' && ((next.role && next.role !== 'owner') || (next.status && next.status !== 'active'))
			if (await @repo.countActiveOwners()) <= 1
				return 'The site needs at least one active owner'
		null

	async updateUser(id: number, input: Record<string, any>, actor: any)
		target := await @getUser id
		return { ok: false, error: 'User not found' } unless target
		fields := {} as Record<string, any>
		fields.name = String(input.name).trim() if input.name !== undefined
		fields.bio = String(input.bio ?? '').slice(0, 2000) if input.bio !== undefined
		if input.avatar !== undefined
			n := Number(input.avatar)
			fields.avatar_media_id = n > 0 ? n : null
		if input.email !== undefined
			addr := String(input.email).trim().toLowerCase()
			unless /^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(addr)
				return { ok: false, error: 'Enter a valid email', field: 'email' }
			if addr !== target.email
				other := await @repo.byEmail addr
				if other
					return { ok: false, error: 'Another account uses that email', field: 'email' }
				fields.email = addr
		if input.role !== undefined && input.role !== target.role
			role := await @repo.role String(input.role)
			return { ok: false, error: 'Unknown role', field: 'role' } unless role
			return { ok: false, error: 'That role is for a different kind of user', field: 'role' } if role.kind !== target.kind
			fields.role = role.id
		if input.status !== undefined && input.status !== target.status
			return { ok: false, error: 'Unknown status', field: 'status' } unless ['active', 'suspended'].includes(input.status)
			fields.status = input.status
		if target.kind === 'staff' && input.slug !== undefined
			wanted := slugifyName String(input.slug || fields.name || target.name || target.email.split('@')[0])
			if wanted && wanted !== target.slug
				if await @repo.slugTaken(wanted, id)
					return { ok: false, error: 'That author URL is taken', field: 'slug' }
				fields.slug = wanted
		problem := await @guard actor, target, { role: fields.role, status: fields.status }
		return { ok: false, error: problem, field: fields.role ? 'role' : 'status' } if problem
		await @repo.update id, fields
		if fields.status === 'suspended'
			await @repo.deleteSessions id
		changed := Object.keys(fields)
		if changed.length
			await @audit actor?.id ?? null, actor?.email ?? '', 'user.update', target.email, { fields: changed, role: fields.role, status: fields.status }
		{ ok: true }

	async setPassword(id: number, password: string, actor: any, keepSession: string | null = null)
		user := await @getUser id
		return { ok: false, error: 'User not found' } unless user
		problem := passwordProblem password, user.email
		return { ok: false, error: problem, field: 'password' } if problem
		await @repo.setPassword id, hashPassword(password)
		await @repo.deleteSessions id, keepSession
		await @audit actor?.id ?? id, actor?.email ?? user.email, 'user.password', user.email, {}
		{ ok: true }

	async changePassword(user: any, current: string, next: string)
		row := await @repo.byId user.id
		return { ok: false, error: 'Current password is wrong', field: 'current' } unless row && verifyPassword(current, row.password_hash)
		@setPassword user.id, next, user, user.sessionId

	// --- one-time links ----------------------------------------------------

	async issueToken(userId: number, purpose: string, ttlMs: number)
		raw := randomBytes(32).toString('base64url')
		await @repo.createToken sha256(raw), userId, purpose, Date.now() + ttlMs
		raw

	async peekToken(raw: string, purpose: string)
		tok := await @repo.token sha256(raw), purpose
		return undefined unless tok
		user := await @getUser tok.user_id
		user ? { token: tok, user } : undefined

	// Staff create an invited account; the invitee picks a password from the
	// link. With the log mail provider the link is returned so the admin can
	// hand it over directly.
	async invite(input: { email: string, name: string, role: string }, actor: any)
		addr := input.email.trim().toLowerCase()
		return { ok: false, error: 'Enter a valid email', field: 'email' } unless /^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(addr)
		if await @repo.byEmail(addr)
			return { ok: false, error: 'An account with that email exists', field: 'email' }
		role := await @repo.role input.role
		return { ok: false, error: 'Pick a role', field: 'role' } unless role
		return { ok: false, error: 'Only an owner can invite owners', field: 'role' } if role.id === 'owner' && actor?.role !== 'owner'
		id := await @createUser addr, input.name || '', randomBytes(24).toString('hex'), role.id, { kind: role.kind, status: 'invited', verified: false }
		raw := await @issueToken id, 'invite', 7 * DAY
		link := `${siteOrigin()}/invite/${raw}`
		site := await settings(@c).get 'site'
		sent := await mail(@c).send {
			to: addr
			subject: `You're invited to ${site.title}`
			template: 'invite'
			text: `${actor?.name || actor?.email || 'An administrator'} invited you to ${site.title} as ${role.label}.\n\nChoose your password here (the link works for 7 days):\n${link}\n\nIf you did not expect this, ignore this email.`
		}
		await @audit actor?.id ?? null, actor?.email ?? '', 'user.invite', addr, { role: role.id }
		{ ok: true, id, link: (sent.provider === 'log' || !sent.ok) ? link : null, mailed: sent.ok, mailError: sent.ok ? null : sent.error }

	async acceptInvite(raw: string, name: string, password: string)
		found := await @peekToken raw, 'invite'
		return { ok: false, error: 'This invitation is invalid or has expired' } unless found
		problem := passwordProblem password, found.user.email
		return { ok: false, error: problem, field: 'password' } if problem
		await @repo.useToken found.token.id
		await @repo.setPassword found.user.id, hashPassword(password)
		fields := { status: 'active', email_verified_at: Date.now() } as Record<string, any>
		if name.trim()
			fields.name = name.trim()
			if found.user.kind === 'staff'
				fields.slug = await @uniqueUserSlug(name, found.user.id)
		await @repo.update found.user.id, fields
		await @audit found.user.id, found.user.email, 'user.invite_accepted', found.user.email, {}
		{ ok: true, user: found.user }

	// Always reports success, so the form does not reveal which emails exist.
	async requestReset(email: string, ip = '')
		addr := email.trim().toLowerCase()
		return { ok: true } unless await @rateLimit('reset:ip', ip || 'unknown', 10, 60 * 60 * 1000)
		return { ok: true } unless await @rateLimit('reset:email', addr, 3, 60 * 60 * 1000)
		row := await @repo.byEmail addr
		if row && row.status === 'active'
			raw := await @issueToken row.id, 'reset', 60 * 60 * 1000
			locale := row.kind === 'client' ? (row.locale || 'en') : 'en'
			site := await settings(@c).localized 'site', locale
			t := await tFor @c, locale
			await mail(@c).send {
				to: addr
				subject: t('email.reset.subject', { site: site.title })
				template: 'reset'
				text: t('email.reset.text', { email: addr, link: `${siteOrigin()}/reset/${raw}` })
			}
			await @audit row.id, addr, 'user.reset_requested', addr, {}
		{ ok: true }

	async resetPassword(raw: string, password: string)
		found := await @peekToken raw, 'reset'
		return { ok: false, error: 'This link is invalid or has expired' } unless found
		problem := passwordProblem password, found.user.email
		return { ok: false, error: problem, field: 'password' } if problem
		await @repo.useToken found.token.id
		await @repo.setPassword found.user.id, hashPassword(password)
		await @repo.deleteSessions found.user.id
		// Following an emailed link proves the address.
		unless found.user.email_verified_at
			await @repo.update found.user.id, { email_verified_at: Date.now() }
		await @audit found.user.id, found.user.email, 'user.password_reset', found.user.email, {}
		{ ok: true, user: found.user }

	// --- client accounts -----------------------------------------------------

	async register(input: { email: string, name: string, password: string }, ip = '')
		site := await settings(@c).get 'site'
		return { ok: false, error: 'Registration is closed' } unless site.registration === 'open'
		return { ok: false, error: 'Too many sign-ups from your network; try again later' } unless await @rateLimit('register:ip', ip || 'unknown', 5, 60 * 60 * 1000)
		addr := input.email.trim().toLowerCase()
		return { ok: false, error: 'Enter a valid email', field: 'email' } unless /^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(addr)
		problem := passwordProblem input.password, addr
		return { ok: false, error: problem, field: 'password' } if problem
		existing := await @repo.byEmail addr
		locale := String(input.locale || getScope()?.event?.locals?.locale?.language || 'en')
		if existing
			// Same answer as success; tell the owner of the address instead.
			t := await tFor @c, locale
			localSite := await settings(@c).localized 'site', locale
			await mail(@c).send { to: addr, subject: t('email.exists.subject', { site: localSite.title }), template: 'register_exists', text: t('email.exists.text', { email: addr, login: `${siteOrigin()}/account/login`, forgot: `${siteOrigin()}/account/forgot` }) }
			return { ok: true }
		id := await @createUser addr, input.name ?? '', input.password, 'member', { kind: 'client', verified: false }
		await @repo.update id, { locale }
		await @sendVerification id
		await @audit id, addr, 'user.register', addr, {}
		{ ok: true }

	async sendVerification(userId: number)
		user := await @getUser userId
		return unless user && !user.email_verified_at
		raw := await @issueToken userId, 'verify', 2 * DAY
		locale := (user as any).locale || 'en'
		site := await settings(@c).localized 'site', locale
		t := await tFor @c, locale
		await mail(@c).send { to: user.email, subject: t('email.verify.subject', { site: site.title }), template: 'verify', text: t('email.verify.text', { link: `${siteOrigin()}/verify/${raw}` }) }
		return

	async resendVerification(email: string, ip = '')
		return unless await @rateLimit('verify:ip', ip || 'unknown', 5, 60 * 60 * 1000)
		row := await @repo.byEmail email.trim().toLowerCase()
		if row && row.kind === 'client' && row.status === 'active'
			await @sendVerification row.id
		return

	async verifyEmail(raw: string)
		found := await @peekToken raw, 'verify'
		return { ok: false } unless found
		await @repo.useToken found.token.id
		await @repo.update found.user.id, { email_verified_at: Date.now() }
		await @audit found.user.id, found.user.email, 'user.verified', found.user.email, {}
		{ ok: true, user: found.user }

	// Self-service deletion for clients: anonymised now, row purged after 30
	// days by maintenance.
	async deleteOwnAccount(user: any, password: string)
		row := await @repo.byId user.id
		return { ok: false, error: 'Password is wrong', field: 'password' } unless row && verifyPassword(password, row.password_hash)
		return { ok: false, error: 'Staff accounts are removed by an administrator' } if row.kind !== 'client'
		await @repo.update row.id, { status: 'deleted', deleted_at: Date.now(), email: `deleted-${row.id}@invalid`, name: '', bio: '', slug: null }
		await @repo.deleteSessions row.id
		await @audit row.id, row.email, 'user.self_delete', `user#${row.id}`, {}
		{ ok: true }

	// Hard delete by staff (after content was reassigned by the caller).
	async deleteUser(id: number, actor: any)
		target := await @getUser id
		return { ok: false, error: 'User not found' } unless target
		return { ok: false, error: 'You cannot delete yourself' } if actor?.id === id
		problem := await @guard actor, target, { status: 'deleted' }
		return { ok: false, error: problem } if problem
		await @repo.remove id
		await @audit actor?.id ?? null, actor?.email ?? '', 'user.delete', target.email, { kind: target.kind }
		{ ok: true }

	async exportData(userId: number)
		user := await @getUser userId
		{
			exportedAt: new Date().toISOString()
			profile: user
			sessions: await @repo.sessions(userId)
			activity: await @repo.auditList({ actorId: userId, limit: 1000 })
		}

	// --- two-factor --------------------------------------------------------

	async beginTotp(user: any)
		secret := newSecret()
		await @repo.update user.id, { totp_secret: sealSecret(secret, @appSecret()), totp_enabled: 0 }
		site := await settings(@c).get 'site'
		{ secret, uri: otpauthUri(secret, user.email, site.title || 'Norns CMS') }

	async confirmTotp(user: any, code: string)
		row := await @repo.byId user.id
		secret := openSecret row?.totp_secret ?? null, @appSecret()
		return { ok: false, error: 'Start the setup again' } unless secret
		return { ok: false, error: 'That code is not right; check the time on your device' } unless verifyTotp(secret, code)
		codes := recoveryCodes()
		await @repo.update user.id, { totp_enabled: 1, recovery_codes: JSON.stringify(codes.map((c) => sha256(c))) }
		await @audit user.id, user.email, 'user.2fa_enabled', user.email, {}
		{ ok: true, codes }

	async disableTotp(user: any, password: string)
		row := await @repo.byId user.id
		return { ok: false, error: 'Password is wrong', field: 'password' } unless row && verifyPassword(password, row.password_hash)
		await @repo.update user.id, { totp_enabled: 0, totp_secret: null, recovery_codes: '[]' }
		await @audit user.id, user.email, 'user.2fa_disabled', user.email, {}
		{ ok: true }

	async resetTotp(id: number, actor: any)
		target := await @getUser id
		return { ok: false, error: 'User not found' } unless target
		problem := await @guard actor, target, {}
		return { ok: false, error: problem } if problem
		await @repo.update id, { totp_enabled: 0, totp_secret: null, recovery_codes: '[]' }
		await @audit actor?.id ?? null, actor?.email ?? '', 'user.2fa_reset', target.email, {}
		{ ok: true }

	// --- roles -------------------------------------------------------------

	roles(kind?: string)
		@repo.roles kind

	role(id: string)
		@repo.role id

	async saveRole(input: { id?: string, kind: string, label: string, description: string, capabilities: string[] }, actor: any)
		label := String(input.label ?? '').trim()
		return { ok: false, error: 'Name the role', field: 'label' } unless label
		id := input.id || slugifyName(label).replace(/-/g, '_')
		existing := await @repo.role id
		return { ok: false, error: 'Built-in roles cannot be changed' } if existing?.builtin
		return { ok: false, error: 'A role with that name exists', field: 'label' } if !input.id && existing
		known := @declaredCapabilities().map((cap) => cap.id)
		caps := (input.capabilities ?? []).filter((cap) => known.includes(cap))
		caps.unshift('admin.access') if input.kind === 'staff' && !caps.includes('admin.access')
		await @repo.saveRole { id, kind: input.kind === 'client' ? 'client' : 'staff', label, description: String(input.description ?? ''), capabilities: caps }
		await @audit actor?.id ?? null, actor?.email ?? '', 'role.save', id, { capabilities: caps }
		{ ok: true, id }

	async deleteRole(id: string, actor: any)
		role := await @repo.role id
		return { ok: false, error: 'Role not found' } unless role
		return { ok: false, error: 'Built-in roles cannot be deleted' } if role.builtin
		if (await @repo.roleUsage(id)) > 0
			return { ok: false, error: 'Move its users to another role first' }
		await @repo.deleteRole id
		await @audit actor?.id ?? null, actor?.email ?? '', 'role.delete', id, {}
		{ ok: true }

	grants(role: any, cap: string)
		capMatches role?.capabilities ?? [], cap

	// --- audit -------------------------------------------------------------

	async audit(actorId: number | null, actor: string, action: string, target: string, meta: Record<string, any> = {})
		try
			await @repo.audit actorId, actor, action, target, meta
		catch e
			console.error 'audit: could not write', action, e
		return

	auditList(q: Record<string, any> = {})
		@repo.auditList q

	async purge(): Promise<Record<string, number>>
		sec := await @security()
		days := Number(sec.auditRetentionDays) || 365
		{
			audit: await @repo.purgeAudit(Date.now() - days * DAY)
			sessions: await @repo.purgeSessions()
			tokens: await @repo.purgeTokens()
			rateLimits: await @repo.purgeRateLimits(Date.now() - DAY)
			deletedUsers: await @repo.purgeDeleted(Date.now() - 30 * DAY)
		}
