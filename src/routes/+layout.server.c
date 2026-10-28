import { protection } from '$lib/norns/protection/server/public'

// Form protection settings for FormGuard on every page (any form anywhere).
export load := async ({ locals }: any) =>
	{ guard: await protection(locals.container).clientConfig() }
