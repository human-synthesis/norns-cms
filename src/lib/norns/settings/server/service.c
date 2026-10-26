import * as v from 'valibot'
import type { SettingsRegistry } from './registry'

// Per-isolate cache of the whole settings table, keyed by the underlying
// database object (D1 binding or SQLite handle) so tests with separate
// in-memory databases never share entries. Settings are read on every
// request by handles (security, consent); a short TTL keeps D1 reads low
// while letting another isolate's save show up quickly.
TTL := 15000
cache := new WeakMap<any, { at: number, rows: Record<string, any> }>()

export class SettingsService
	registry: SettingsRegistry
	db: any
	compile: (fields: any[]) => any
	constructor(@registry: SettingsRegistry, @db: any, @compile: (fields: any[]) => any)

	groups()
		@registry.all()

	group(id: string)
		@registry.get id

	defaults(group: any): Record<string, any>
		out := {} as Record<string, any>
		for f of group.fields
			out[f.name] = f.default ?? (f.type === 'boolean' ? false : (f.type === 'tags' || f.type === 'lines' ? [] : ''))
		out

	async rows(): Promise<Record<string, any>>
		key := @db.raw ?? @db
		hit := cache.get key
		return hit.rows if hit && Date.now() - hit.at < TTL
		rows := {} as Record<string, any>
		try
			for r of await @db.all('SELECT "group", value FROM settings', [])
				rows[r.group] = JSON.parse(r.value || '{}')
		catch
			// Table missing (fresh database before migrations): defaults only.
			return rows
		cache.set key, { at: Date.now(), rows }
		rows

	async get(id: string): Promise<Record<string, any>>
		group := @registry.get id
		unless group
			throw new Error(`settings: unknown group "${id}"`)
		stored := (await @rows())[id] ?? {}
		{ ...@defaults(group), ...stored }

	async save(id: string, raw: Record<string, any>, userId: number | null)
		group := @registry.get id
		unless group
			throw new Error(`settings: unknown group "${id}"`)
		{ schema } := @compile group.fields
		result := v.safeParse schema, raw
		unless result.success
			return { ok: false, issues: result.issues }
		await @write id, result.output, userId
		{ ok: true, values: result.output }

	// Merge a partial update without validation; for values set by code
	// (e.g. the consent policy version bump), never for form input.
	async patch(id: string, values: Record<string, any>, userId: number | null)
		next := { ...(await @get id), ...values }
		await @write id, next, userId
		next

	async write(id: string, value: Record<string, any>, userId: number | null)
		await @db.run 'INSERT INTO settings ("group", value, updated_at, updated_by) VALUES (?, ?, ?, ?) ON CONFLICT("group") DO UPDATE SET value = excluded.value, updated_at = excluded.updated_at, updated_by = excluded.updated_by', [id, JSON.stringify(value), Date.now(), userId]
		cache.delete @db.raw ?? @db
		return
