// The management API router. Endpoints are registered by each feature from
// its module.c (registerEndpoint in api/server/public.c), so the router, the
// OpenAPI document and the admin reference all read one list.
//
// Endpoint: {
//   method: 'GET' | 'POST' | 'PATCH' | 'PUT' | 'DELETE',
//   path: '/content/:type/:id',          // `:name` segments become params
//   capability: 'content.view' | null,   // null = any valid key
//   tag: 'Content', summary: '…', description?: '…',
//   query?: [{ name, description }], body?: { description, example },
//   raw?: true,                          // handler reads the body itself (uploads)
//   handler: async ({ c, user, params, query, body, event }) => data | Response
// }
// A handler's return value is sent as { data, meta? } (return { data, meta }
// to set meta). Throw apiError(status, message, extra) for errors.

export class ApiError extends Error {
	constructor(status, message, extra = {}) {
		super(message);
		this.status = status;
		this.extra = extra;
	}
}

export const apiError = (status, message, extra = {}) => new ApiError(status, message, extra);

const compile = (path) => {
	const names = [];
	const pattern = path
		.split('/')
		.map((seg) => {
			if (seg.startsWith(':')) {
				names.push(seg.slice(1));
				return '([^/]+)';
			}
			return seg.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
		})
		.join('/');
	return { re: new RegExp(`^${pattern}/?$`), names };
};

/** Find the endpoint for a method + path; distinguishes 404 from 405. */
export function match(endpoints, method, path) {
	let pathMatched = false;
	for (const ep of endpoints) {
		ep._c ??= compile(ep.path);
		const m = ep._c.re.exec(path);
		if (!m) continue;
		pathMatched = true;
		if (ep.method !== method) continue;
		const params = {};
		ep._c.names.forEach((n, i) => (params[n] = decodeURIComponent(m[i + 1])));
		return { ep, params };
	}
	return pathMatched ? { notAllowed: true } : null;
}

const toOpenApiPath = (p) => p.replace(/:([a-zA-Z]+)/g, '{$1}');

/** An OpenAPI 3.1 document from the registered endpoints. */
export function openapi(endpoints, { origin, title }) {
	const paths = {};
	for (const ep of endpoints) {
		const p = toOpenApiPath(ep.path);
		paths[p] ??= {};
		const params = [
			...(ep._c ?? compile(ep.path)).names.map((name) => ({ name, in: 'path', required: true, schema: { type: 'string' } })),
			...(ep.query ?? []).map((q) => ({ name: q.name, in: 'query', required: false, description: q.description, schema: { type: 'string' } }))
		];
		const op = {
			tags: [ep.tag],
			summary: ep.summary,
			description: [ep.description, ep.capability ? `Requires the \`${ep.capability}\` capability.` : 'Any valid key.'].filter(Boolean).join('\n\n'),
			parameters: params,
			responses: { 200: { description: 'OK — `{ "data": … }`' }, 400: { description: 'Invalid input' }, 401: { description: 'Missing or invalid key' }, 403: { description: 'The key lacks the capability' }, 404: { description: 'Not found' } }
		};
		if (ep.body) {
			op.requestBody = ep.raw
				? { content: { 'application/octet-stream': { schema: { type: 'string', format: 'binary' } } }, description: ep.body.description }
				: { content: { 'application/json': { schema: { type: 'object' }, example: ep.body.example } }, description: ep.body.description };
		}
		paths[p][ep.method.toLowerCase()] = op;
	}
	return {
		openapi: '3.1.0',
		info: { title: `${title} management API`, version: '1' },
		servers: [{ url: `${origin}/api/v1` }],
		components: { securitySchemes: { bearer: { type: 'http', scheme: 'bearer', description: 'An API key from Admin → API' } } },
		security: [{ bearer: [] }],
		paths
	};
}
