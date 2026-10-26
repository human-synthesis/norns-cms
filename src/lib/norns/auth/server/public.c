import type { Container } from '@human-synthesis/norns/server'
import { error, redirect, fail } from '@sveltejs/kit'
import type { AuthService, SessionUser } from './service'
import type { User } from './repo'
import { can, isStaff } from '../shared/can'

export type { User, SessionUser }
export { can, isStaff }
export { passwordProblem, MIN_PASSWORD } from './policy.js'

export COOKIE := 'cms_session'
export MFA_COOKIE := 'cms_mfa'

svc := (c: Container) => c.resolve('auth.service') as AuthService

export auth := (c: Container) => {
	login: (email: string, password: string, meta?: any) => svc(c).login(email, password, meta)
	loginMfa: (token: string, code: string, meta?: any) => svc(c).loginMfa(token, code, meta)
	logout: (sessionId: string) => svc(c).logout(sessionId)
	userForSession: (sessionId: string) => svc(c).userForSession(sessionId)
	sessions: (userId: number) => svc(c).sessions(userId)
	revokeSession: (userId: number, sessionId: string) => svc(c).revokeSession(userId, sessionId)
	revokeAll: (userId: number, exceptId?: string | null) => svc(c).revokeAll(userId, exceptId)
	createUser: (email: string, name: string, password: string, role?: string, opts?: any) => svc(c).createUser(email, name, password, role, opts)
	hasUsers: () => svc(c).hasUsers()
	listUsers: (q?: any) => svc(c).listUsers(q)
	countUsers: (q?: any) => svc(c).countUsers(q)
	getUser: (id: number) => svc(c).getUser(id)
	userByEmail: (email: string) => svc(c).userByEmail(email)
	profiles: (ids: number[]) => svc(c).profiles(ids)
	authorBySlug: (slug: string) => svc(c).authorBySlug(slug)
	updateUser: (id: number, input: any, actor: any) => svc(c).updateUser(id, input, actor)
	setPassword: (id: number, password: string, actor: any) => svc(c).setPassword(id, password, actor)
	changePassword: (user: any, current: string, next: string) => svc(c).changePassword(user, current, next)
	invite: (input: any, actor: any) => svc(c).invite(input, actor)
	peekToken: (raw: string, purpose: string) => svc(c).peekToken(raw, purpose)
	acceptInvite: (raw: string, name: string, password: string) => svc(c).acceptInvite(raw, name, password)
	requestReset: (email: string, ip?: string) => svc(c).requestReset(email, ip)
	resetPassword: (raw: string, password: string) => svc(c).resetPassword(raw, password)
	register: (input: any, ip?: string) => svc(c).register(input, ip)
	resendVerification: (email: string, ip?: string) => svc(c).resendVerification(email, ip)
	verifyEmail: (raw: string) => svc(c).verifyEmail(raw)
	deleteOwnAccount: (user: any, password: string) => svc(c).deleteOwnAccount(user, password)
	deleteUser: (id: number, actor: any) => svc(c).deleteUser(id, actor)
	exportData: (userId: number) => svc(c).exportData(userId)
	beginTotp: (user: any) => svc(c).beginTotp(user)
	confirmTotp: (user: any, code: string) => svc(c).confirmTotp(user, code)
	disableTotp: (user: any, password: string) => svc(c).disableTotp(user, password)
	resetTotp: (id: number, actor: any) => svc(c).resetTotp(id, actor)
	roles: (kind?: string) => svc(c).roles(kind)
	role: (id: string) => svc(c).role(id)
	saveRole: (input: any, actor: any) => svc(c).saveRole(input, actor)
	deleteRole: (id: string, actor: any) => svc(c).deleteRole(id, actor)
	capabilities: () => svc(c).declaredCapabilities()
	rateLimit: (scope: string, value: string, max: number, windowMs: number) => svc(c).rateLimit(scope, value, max, windowMs)
	purge: () => svc(c).purge()
	startSession: (user: any, userAgent?: string) => svc(c).startSession(user, userAgent)
	can: (user: any, cap: string) => can(user, cap)
}

// The audit log, for every feature: audit(c).log(user, 'consent.script_save', 'GA4', { … }).
export audit := (c: Container) => {
	log: (actor: any, action: string, target = '', meta: Record<string, any> = {}) => svc(c).audit(actor?.id ?? null, actor?.email ?? '', action, target, meta)
	list: (q?: any) => svc(c).auditList(q)
}

// Capabilities are declared in code by the feature that checks them:
// registerCapabilities(app, [{ id: 'media.upload', label: 'Upload media', group: 'Media' }]).
export registerCapabilities := (app: Container, caps: { id: string, label: string, group?: string }[]) =>
	for cap of caps
		app.single `auth.cap:${cap.id}`, => cap

// For routes: throw 403 unless the user holds the capability.
export requireCap := (user: any, cap: string) =>
	unless can(user, cap)
		throw error 403, 'You do not have permission to do that'
	user

export setSessionCookie := (event: any, session: { id: string, maxAge: number }) =>
	event.cookies.set COOKIE, session.id, {
		path: '/'
		httpOnly: true
		sameSite: 'lax'
		secure: event.url.protocol === 'https:'
		maxAge: session.maxAge
	}

export clientIp := (event: any) =>
	try
		event.getClientAddress()
	catch
		event.request.headers.get('cf-connecting-ip') ?? 'unknown'

// Only same-site relative paths survive as post-login redirects.
export safeNext := (next: any, fallback: string) =>
	n := String(next ?? '')
	(n.startsWith('/') && !n.startsWith('//') && !n.startsWith('/\\')) ? n : fallback

ACCOUNT_PUBLIC := ['/account/login', '/account/register', '/account/forgot']

// SvelteKit handle wired through boot({ extraHandle }): turns the session
// cookie into event.locals.user and enforces the two audiences.
//
//   /admin/**     staff with admin.access only. Clients get 403; anonymous
//                 GETs go to /login. Staff who must enrol in 2FA are held on
//                 /admin/profile until they do.
//   /account/**   signed-in clients (sign-in, register, forgot are public).
//                 Staff are sent to their admin profile.
//
// The guard lives here and not only in layouts because SvelteKit runs form
// actions BEFORE layout load functions: a layout guard alone leaves every
// `?/action` POST open to anonymous callers.
export sessionHandle := (opts: { protect?: string[] } = {}) =>
	protect := opts.protect ?? ['/admin']
	async ({ event, resolve }: any) =>
		sid := event.cookies.get COOKIE
		user := sid ? await auth(event.locals.container).userForSession(sid) : undefined
		event.locals.user = user ?? null
		path := event.url.pathname
		isGet := event.request.method === 'GET' || event.request.method === 'HEAD'
		under := (p: string) => path === p || path.startsWith(p + '/')
		if protect.some(under)
			unless user
				throw redirect(303, `/login?next=${encodeURIComponent(path + event.url.search)}`) if isGet
				throw error 403, 'Sign in to do that'
			unless isStaff(user)
				throw redirect(303, '/account') if isGet
				throw error 403, 'This area is for staff'
			if user.needs2fa && !under('/admin/profile')
				throw redirect(303, '/admin/profile?enroll=1') if isGet
				throw error 403, 'Set up two-factor sign-in first'
		if under('/account') && !ACCOUNT_PUBLIC.some(under)
			unless user
				throw redirect(303, `/account/login?next=${encodeURIComponent(path)}`) if isGet
				throw error 403, 'Sign in to do that'
			if user.kind === 'staff'
				throw redirect(303, '/admin/profile') if isGet
				throw error 403, 'Staff manage their account in the admin'
		resolve event

LOGIN_ERRORS := {
	invalid: 'Invalid email or password'
	rate: 'Too many attempts. Wait a few minutes and try again.'
	unverified: 'Confirm your email address first: we sent you a link when you registered.'
} as Record<string, string>

// Shared by /login, /login/2fa and /account/login: set the session cookie
// and redirect (staff to the admin, members to their account), go to the
// second step, or return the form error.
export finishLogin := (event: any, r: any, next: any, email = '') =>
	if r.ok
		setSessionCookie event, r.session
		event.cookies.delete MFA_COOKIE, { path: '/' }
		home := r.user.kind === 'staff' ? '/admin' : '/account'
		dest .= safeNext next, home
		dest = home if r.user.kind !== 'staff' && dest.startsWith('/admin')
		throw redirect 303, dest
	if r.mfa
		event.cookies.set MFA_COOKIE, r.mfa, { path: '/', httpOnly: true, sameSite: 'lax', secure: event.url.protocol === 'https:', maxAge: 300 }
		throw redirect 303, `/login/2fa?next=${encodeURIComponent(safeNext(next, '/admin'))}`
	fail 400, {
		errors: [{ path: [{ key: 'password' }], message: LOGIN_ERRORS[r.error] ?? LOGIN_ERRORS.invalid }]
		values: { email }
		unverified: r.error === 'unverified'
	}
