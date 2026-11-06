import { page } from '@human-synthesis/norns/server'
import { fail } from '@sveltejs/kit'
import { blocked } from '$lib/norns/protection/server/public'
import { seo } from '$lib/norns/seo/server/public'
import { requireCap, audit } from '$lib/norns/auth/server/public'

export load := page.load
	handler: async ({ container, url, user }) =>
		requireCap user, 'protection.manage'
		checkPath := url.searchParams.get('check') ?? ''
		{
			paths: await blocked(container).list()
			suggestions: await seo(container).probeSuggestions()
			check: checkPath ? await blocked(container).check(checkPath) : null
			checkPath
		}

form := (event: any) => event.request.formData()

export actions := page.actions
	add:
		run: async ({ container, event, user }) =>
			requireCap user, 'protection.manage'
			fd := await form event
			r := await blocked(container).add { pattern: String(fd.get('pattern') ?? ''), note: String(fd.get('note') ?? '') }, user
			return fail(400, { error: r.error, values: { pattern: fd.get('pattern'), note: fd.get('note') } }) unless r.ok
			await audit(container).log user, 'protection.block_path', r.pattern, {}
			{ saved: r.existed ? `${r.pattern} was already on the list; it is on.` : `Blocked ${r.pattern}.` }
	fromLog:
		run: async ({ container, event, user }) =>
			requireCap user, 'protection.manage'
			fd := await form event
			done := [] as string[]
			cleared .= 0
			for pattern of fd.getAll('pattern').map(String)
				r := await seo(container).blockFromLog pattern, user
				continue unless r.ok
				done.push r.pattern
				cleared += r.cleared
				await audit(container).log user, 'protection.block_path', r.pattern, { from: '404', cleared: r.cleared }
			{ saved: done.length ? `Blocked ${done.join(', ')}; ${cleared} path${cleared === 1 ? '' : 's'} left the 404 log.` : 'Nothing to block.' }
	toggle:
		run: async ({ container, event, user }) =>
			requireCap user, 'protection.manage'
			fd := await form event
			row := await blocked(container).update Number(fd.get('id')), { enabled: fd.get('enabled') === '1' }
			return fail(404, { error: 'Not found' }) unless row
			await audit(container).log user, 'protection.block_path_update', row.pattern, { enabled: !!row.enabled }
			{ saved: `${row.pattern} is ${row.enabled ? 'blocked again' : 'paused'}.` }
	remove:
		run: async ({ container, event, user }) =>
			requireCap user, 'protection.manage'
			fd := await form event
			await blocked(container).remove Number(fd.get('id'))
			await audit(container).log user, 'protection.unblock_path', `path#${fd.get('id')}`, {}
			{ saved: 'Removed from the list; the path is served normally again.' }
	defaults:
		run: async ({ container, user }) =>
			requireCap user, 'protection.manage'
			added := await blocked(container).restoreDefaults()
			await audit(container).log user, 'protection.block_defaults', `${added} patterns`, {}
			{ saved: added ? `Added ${added} pattern${added === 1 ? '' : 's'} from the starting list.` : 'The whole starting list is already there.' }
