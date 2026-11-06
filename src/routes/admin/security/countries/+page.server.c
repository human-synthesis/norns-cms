import { page } from '@human-synthesis/norns/server'
import { fail } from '@sveltejs/kit'
import { countryFilter, countryOf } from '$lib/norns/protection/server/public'
import { requireCap, audit } from '$lib/norns/auth/server/public'

export load := page.load
	handler: async ({ container, event, user }) =>
		requireCap user, 'protection.manage'
		countryFilter(container).status event

export actions := page.actions
	save:
		run: async ({ container, event, user }) =>
			requireCap user, 'protection.manage'
			fd := await event.request.formData()
			input := { mode: fd.get('mode'), countries: fd.getAll('countries').map(String), unknown: fd.get('unknown'), admin: fd.has('admin') }
			r := await countryFilter(container).save input, { user, country: countryOf(event) }
			return fail(400, { error: r.error, values: input }) unless r.ok
			await audit(container).log user, 'protection.countries', r.filter.mode, { countries: r.filter.countries.length, admin: r.filter.admin }
			{ saved: r.filter.mode === 'off' ? 'Country filter is off: every country can visit.' : `Saved. ${r.filter.countries.length} countr${r.filter.countries.length === 1 ? 'y' : 'ies'} ${r.filter.mode === 'allow' ? 'allowed' : 'blocked'}; it applies within a minute everywhere.` }
	clearStats:
		run: async ({ container, user }) =>
			requireCap user, 'protection.manage'
			await countryFilter(container).clearStats()
			await audit(container).log user, 'protection.countries_stats_clear', '', {}
			{ saved: 'Counts reset.' }
