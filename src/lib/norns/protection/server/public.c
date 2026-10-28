import type { Container } from '@human-synthesis/norns/server'
import { fail } from '@sveltejs/kit'
import type { ProtectionService, FormDef } from './service'

export type { FormDef }
export { HONEYPOT, TOKEN, TURNSTILE_FIELD } from '../shared/guard.js'

svc := (c: Container) => c.resolve('protection.service') as ProtectionService

// Form protection: every public form is registered here, gets a managed
// honeypot and signed render time, and Cloudflare Turnstile when keys are
// set (Admin → Form protection). Outcomes are counted per form and day.
export protection := (c: Container) => {
	config: () => svc(c).config()
	saveConfig: (input: any, actor: any, meta?: any) => svc(c).saveConfig(input, actor, meta)
	forms: () => svc(c).forms()
	saveForm: (id: string, input: any, actor: any, meta?: any) => svc(c).saveForm(id, input, actor, meta)
	definitions: () => svc(c).definitions()
	clientConfig: () => svc(c).clientConfig()
	check: (event: any, formId: string, fd: FormData) => svc(c).check(event, formId, fd)
	message: (reason: string, event: any, formId: string) => svc(c).message(reason, event, formId)
	purge: () => svc(c).purge()
}

// A form a feature owns, declared from its module.c:
// registerForm(app, { id: 'auth.member-login', label: 'Member sign-in', group: 'Accounts', path: '/account/login', minMs: 0 }).
export registerForm := (app: Container, def: FormDef) =>
	app.single `protection.form:${def.id}`, => def

// Forms made at runtime (one per row, like Forms): fn(c) → FormDef[].
export registerFormSource := (app: Container, id: string, fn: (c: Container) => Promise<FormDef[]>) =>
	app.single `protection.source:${id}`, => fn

// Wraps SvelteKit actions (page.actions output): each named action checks
// its form first. Reads a clone of the body so the action reads it as usual.
// Failure: fail(400, { guard, guardForm, values }) — FormGuard shows it.
export protect := (actions: Record<string, any>, ids: Record<string, string>) =>
	out := { ...actions }
	for name of Object.keys(ids)
		inner := actions[name]
		formId := ids[name]
		throw new Error(`protect(): no action "${name}"`) unless inner
		out[name] = async (event: any) =>
			c := event.locals.container
			fd := await event.request.clone().formData()
			v := await svc(c).check event, formId, fd
			return inner(event) if v.ok
			values := {} as Record<string, string>
			for [k, val] of fd.entries()
				values[k] = val if typeof val === 'string' && !k.startsWith('_') && !/password|confirm|code|turnstile/i.test(k)
			fail 400, { guard: await svc(c).message(v.reason, event, formId), guardForm: formId, values }
	out
