import type { Container } from '@human-synthesis/norns/server'
import type { ConsentService } from './service'

export { COOKIE } from '../shared/config'

svc := (c: Container) => c.resolve('consent.service') as ConsentService

export consent := (c: Container) => {
	state: (event: any) => svc(c).state(event)
	banner: (state: any) => svc(c).banner(state)
	injection: (state: any, nonce: string) => svc(c).injection(state, nonce)
	record: (event: any, input: any, user: any) => svc(c).record(event, input, user)
	declaration: () => svc(c).declaration()
	providers: () => svc(c).providers()
	scripts: () => svc(c).scripts()
	script: (id: number) => svc(c).script(id)
	saveScript: (id: number | null, raw: any, user: any) => svc(c).saveScript(id, raw, user)
	removeScript: (id: number) => svc(c).removeScript(id)
	bumpVersion: (user: any) => svc(c).bumpVersion(user)
	records: (q?: any) => svc(c).records(q)
	stats: (days?: number) => svc(c).stats(days)
	purge: () => svc(c).purge()
}

// Public pages only: no third-party code in the admin, the account area or
// sign-in pages.
SKIP := /^\/(admin|account|login|reset|invite|verify|api|forms)(\/|$)/

// Works out the visitor's consent state (cookie, region, GPC) into
// event.locals.consent, and injects the consented scripts plus the loader
// into public HTML pages. Must run inside securityHandle so locals.nonce is
// set.
export consentHandle := =>
	async ({ event, resolve }: any) =>
		path := event.url.pathname
		return resolve(event) if SKIP.test(path) || path.startsWith('/_app/')
		c := event.locals.container
		state := await svc(c).state event
		event.locals.consent = state
		inject .= null as any
		resolve event, {
			transformPageChunk: async ({ html }: any) =>
				return html unless html.includes('</head>') || html.includes('</body>')
				inject ??= await svc(c).injection(state, event.locals.nonce ?? '')
				out .= html
				out = out.replace('</head>', `${inject.head}</head>`) if inject.head && out.includes('</head>')
				out = out.replace('</body>', `${inject.body}</body>`) if out.includes('</body>')
				out
		}
