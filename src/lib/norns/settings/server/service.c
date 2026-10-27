import * as v from 'valibot'
import { getScope } from '@human-synthesis/norns/server'
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

	translatable(group: any): string[]
		group.fields.filter((f: any) => f.translatable).map((f: any) => f.name)

	// Values as a language sees them: translatable fields overridden by the
	// language's translation, following the request's fallback chain
	// (de-CH → de → base). Without a language, the base values.
	async localized(id: string, locale: string | null = null): Promise<Record<string, any>>
		base := await @get id
		loc := getScope()?.event?.locals?.locale
		code := locale ?? loc?.language
		return base unless code
		chain := locale ? [locale, ...(locale.includes('-') ? [locale.split('-')[0]] : [])] : (loc?.chain ?? [code])
		translations := (base._i18n ?? {}) as Record<string, any>
		out := { ...base }
		for c of [...chain].reverse()
			for [k, value] of Object.entries(translations[c] ?? {})
				out[k] = value if value !== '' && value != null
		out

	// Save the translatable fields of a group for one language. Empty
	// values fall back to the base value.
	async saveTranslation(id: string, locale: string, raw: Record<string, any>, userId: number | null)
		group := @registry.get id
		unless group
			throw new Error(`settings: unknown group "${id}"`)
		current := await @get id
		translations := { ...(current._i18n ?? {}) } as Record<string, any>
		own := {} as Record<string, string>
		for name of @translatable(group)
			value := String(raw[name] ?? '').trim()
			own[name] = value if value
		if Object.keys(own).length
			translations[locale] = own
		else
			delete translations[locale]
		await @write id, { ...current, _i18n: translations }, userId
		{ ok: true, values: own }

	async save(id: string, raw: Record<string, any>, userId: number | null)
		group := @registry.get id
		unless group
			throw new Error(`settings: unknown group "${id}"`)
		{ schema } := @compile group.fields
		result := v.safeParse schema, raw
		unless result.success
			return { ok: false, issues: result.issues }
		// Translations are kept when the base values change.
		stored := (await @rows())[id] ?? {}
		value := stored._i18n ? { ...result.output, _i18n: stored._i18n } : result.output
		await @write id, value, userId
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
