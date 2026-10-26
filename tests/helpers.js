// Boots the CMS features through the DI container exactly the way boot()
// wires them — no HTTP, no Vite. Each bootApp() gets its own in-memory
// SQLite with the real migrations applied (ordered by file name across
// features, like `norns migrate` and wrangler's flat mirror). Outside a
// request scope there is no D1 binding, so `db` resolves to SQLite.
import { readdirSync, readFileSync, statSync } from 'node:fs';
import { join } from 'node:path';
import { createApp } from '@human-synthesis/norns/server';

process.env.DATABASE_URL = 'file::memory:';
process.env.MEDIA_DIR = join(import.meta.dir, '..', 'data', 'test-media');

const FEATURES = join(import.meta.dir, '..', 'src', 'lib', 'norns');
export const modules = await Promise.all(
	readdirSync(FEATURES)
		.filter((f) => {
			try {
				return statSync(join(FEATURES, f, 'server', 'module.c')).isFile();
			} catch {
				return false;
			}
		})
		.sort()
		.map(async (f) => [f, (await import(join(FEATURES, f, 'server', 'module.c'))).default])
);

export async function migrate(db) {
	const root = join(import.meta.dir, '..', 'migrations');
	const files = [];
	for (const feature of readdirSync(root)) {
		if (feature === 'd1' || !statSync(join(root, feature)).isDirectory()) continue;
		for (const f of readdirSync(join(root, feature))) if (f.endsWith('.sql')) files.push({ feature, f });
	}
	files.sort((a, b) => a.f.localeCompare(b.f) || a.feature.localeCompare(b.feature));
	for (const { feature, f } of files) await db.exec(readFileSync(join(root, feature, f), 'utf8'));
}

export async function bootApp(order) {
	const app = createApp();
	const list = order ?? modules.map(([, m]) => m);
	for (const register of list) register(app);
	await migrate(app.resolve('db'));
	return app.scope();
}

/** A minimal SvelteKit-like event for handle/service code that reads one. */
export function fakeEvent({ country = '', gpc = false, cookies = {}, path = '/' } = {}) {
	const headers = new Headers();
	if (gpc) headers.set('sec-gpc', '1');
	return {
		url: new URL(`http://localhost${path}`),
		platform: { cf: { country } },
		request: { headers, method: 'GET' },
		cookies: { get: (k) => cookies[k] },
		locals: {}
	};
}
