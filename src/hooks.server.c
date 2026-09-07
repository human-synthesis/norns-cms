import { boot } from '@human-synthesis/norns/server'
import { tronSerializer } from '@human-synthesis/norns-tron/server'
import { sessionHandle } from '$lib/norns/auth/server/public'

// Every feature's module.c is eager-loaded at build time so boot() can
// register its bindings up front. Add a feature by adding a folder.
features := import.meta.glob './lib/norns/*/server/module.c', { eager: true }

// sessionHandle runs after boot's contextHandle, so it can resolve the auth
// service from the request container and set event.locals.user. It also
// rejects anonymous requests under /admin for every HTTP method, which the
// layout guard alone cannot do (actions run before layout load).
app := await boot {
	features,
	serializer: tronSerializer(),
	extraHandle: [sessionHandle({ protect: ['/admin'] })],
}

{ handle, handleError } := app
export { handle, handleError }
