import { randomBytes, createHash } from 'node:crypto'
import type { Container } from '@human-synthesis/norns/server'
import type { ApiKeysRepo } from './repo'
import { auth, can, audit } from '../../auth/server/public'
import { capMatches } from '../../auth/shared/can'

KEY_RE := /^ncms_([a-z0-9]{8})_([A-Za-z0-9_-]{32,})$/
sha := (s: string) => createHash('sha256').update(s).digest('hex')

export class ApiKeysService
	repo: ApiKeysRepo
	c: Container
	constructor(@repo: ApiKeysRepo, @c: Container)

	// Capabilities a creator may hand to a key: the declared ones they hold.
	// '*' ("everything I can do") only for creators who hold '*' themselves.
	grantable(user: any): string[]
		declared := auth(@c).capabilities().map((cap: any) => cap.id)
		declared.filter((id: string) => can(user, id))

	async create(input: { name: string, capabilities: string[], expiresInDays?: number | null }, creator: any)
		name := String(input.name ?? '').trim().slice(0, 80)
		return { ok: false, error: 'Name the key', field: 'name' } unless name
		asked := [...new Set((input.capabilities ?? []).map(String))]
		return { ok: false, error: 'Give the key at least one capability', field: 'capabilities' } unless asked.length
		allowed := @grantable creator
		caps := [] as string[]
		for cap of asked
			if cap === '*'
				return { ok: false, error: 'Only users with full access can create full-access keys', field: 'capabilities' } unless capMatches(creator.capabilities ?? [], '*')
				caps.push '*'
			else if allowed.includes(cap)
				caps.push cap
			else
				return { ok: false, error: `You cannot grant "${cap}"`, field: 'capabilities' }
		days := Number(input.expiresInDays) || 0
		prefix := randomBytes(6).toString('hex').slice(0, 8)
		key := `ncms_${prefix}_${randomBytes(24).toString('base64url')}`
		id := await @repo.insert { name, prefix, hash: sha(key), userId: creator.id, capabilities: caps, expiresAt: days > 0 ? Date.now() + days * 86400000 : null }
		await audit(@c).log creator, 'api.key_create', name, { id, capabilities: caps, expiresInDays: days || null }
		{ ok: true, id, key }

	list(userId: number | null = null)
		@repo.list userId

	get(id: number)
		@repo.get id

	async revoke(id: number, actor: any)
		k := await @repo.get id
		return { ok: false, error: 'Key not found' } unless k
		await @repo.revoke id
		await audit(@c).log actor, 'api.key_revoke', k.name, { id }
		{ ok: true }

	// Bearer token → the principal the request acts as, or null. The key
	// acts for its creator: their id (authorship, audit) and role (owner
	// rules), with capabilities = the key's ∩ what the creator holds now.
	async verify(raw: string)
		m := KEY_RE.exec(String(raw ?? '').trim())
		return null unless m
		k := await @repo.byHash sha(raw.trim())
		return null unless k && !k.revoked_at && (!k.expires_at || k.expires_at > Date.now())
		creator := await auth(@c).getUser k.user_id
		return null unless creator && creator.kind === 'staff' && creator.status === 'active'
		role := await auth(@c).role creator.role
		held := role?.capabilities ?? []
		effective := if k.capabilities.includes('*')
			held
		else
			k.capabilities.filter((cap: string) => capMatches(held, cap))
		await @repo.touch(k.id) if !k.last_used_at || Date.now() - k.last_used_at > 60000
		{
			...creator
			email: `${creator.email} (API key "${k.name}")`
			userEmail: creator.email
			capabilities: effective
			needs2fa: false
			sessionId: null
			apiKey: { id: k.id, name: k.name }
		}
