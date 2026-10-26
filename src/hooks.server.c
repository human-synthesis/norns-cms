import { dev } from '$app/environment'
import { boot } from '@human-synthesis/norns/server'
import { tronSerializer } from '@human-synthesis/norns-tron/server'
import { sessionHandle } from '$lib/norns/auth/server/public'
import { securityHandle, settings } from '$lib/norns/settings/server/public'
import { seoHandle } from '$lib/norns/seo/server/public'
import { consentHandle } from '$lib/norns/consent/server/public'

// Every feature's module.c is eager-loaded at build time so boot() can
// register its bindings up front. Add a feature by adding a folder.
features := import.meta.glob './lib/norns/*/server/module.c', { eager: true }

// Order matters (each wraps the next):
//   securityHandle  per-request nonce on every <script>, CSP + headers
//   sessionHandle   cookie → locals.user; guards /admin (staff) and /account
//                   (clients) for every method — actions run before layout
//                   loads, so a layout guard alone would not protect them
//   seoHandle       redirects, /<path>.md twins, 404 log
//   consentHandle   consent state → locals.consent; injects consented
//                   scripts and the loader into public pages
app := await boot {
	features,
	serializer: tronSerializer(),
	extraHandle: [
		securityHandle({ dev, mode: (event) => settings(event.locals.container).get('security').then((s) => s.csp) })
		sessionHandle({ protect: ['/admin'] })
		seoHandle()
		consentHandle()
	],
}

{ handle, handleError } := app
export { handle, handleError }
