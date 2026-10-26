import { handleApi } from '$lib/norns/api/server/public'

// The management API. Every endpoint is registered by its feature
// (registerEndpoint); the reference lives at /admin/developer/docs and
// /api/v1/openapi.json.
handler := (event: any) => handleApi event, '/' + (event.params.path ?? '')

export GET := handler
export POST := handler
export PATCH := handler
export PUT := handler
export DELETE := handler
export OPTIONS := handler
