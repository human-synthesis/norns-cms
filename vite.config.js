import { defineConfig } from 'vite';
import { sveltekit } from '@sveltejs/kit/vite';
import adapter from '@sveltejs/adapter-cloudflare';
import tailwindcss from '@tailwindcss/vite';
import { nornsAutoImport } from '@human-synthesis/norns/auto-import';
import { nornsConfig } from '@human-synthesis/norns/config';
import { nornsPreprocess } from '@human-synthesis/norns/preprocess';
import { nornsCivetPlugin, pugTailwindExtract } from '@human-synthesis/norns/vite';
import { presetUI } from '@human-synthesis/norns-ui/auto-import';

const ui = presetUI();

// SvelteKit 3 has no svelte.config.js: the Kit options go to the sveltekit()
// plugin, built by nornsConfig() (.n components, .c modules and hooks, Pug +
// Civet). nornsCivetPlugin() also provides the $lib alias Kit 3 dropped.
export default defineConfig({
	plugins: [
		nornsCivetPlugin(),
		// Tailwind v4's content scanner doesn't understand Pug's chained-class
		// shorthand (`.flex.items-center.p-4` reads as one dotted token and
		// gets dropped). This plugin walks `.n` files, extracts class candidates,
		// and writes them to node_modules/.cache/norns/tailwind-pug-classes.html,
		// which app.css references via `@source`.
		pugTailwindExtract(),
		// Vite-plugin half of the auto-importer (for standalone .c modules).
		// The Svelte-preprocessor half (for .n / .svelte files) is in the
		// sveltekit() preprocess list below. `helpers` is omitted so the
		// defaults apply; `exportGlobs` is not enabled, so project code
		// (facades, schemas, services) is always imported explicitly.
		nornsAutoImport({
			components: ui.components
		}),
		tailwindcss(),
		sveltekit(
			nornsConfig({
				preprocess: [
					...nornsPreprocess(),
					nornsAutoImport({
						componentDirs: ['src/lib/components', 'src/routes'],
						components: ui.components
					})
				],
				// Cloudflare Workers + D1 (see wrangler.toml). In dev the adapter
				// emulates `cloudflare:workers` from the same config, so `env.DB`
				// resolves to a local D1 under .wrangler/state/. Without it (bun
				// test, a plain Bun server) the app falls back to the SQLite file
				// at DATABASE_URL.
				adapter: adapter()
			})
		)
	],
	// In workspace mode @human-synthesis/norns is a symlink into the kit fork,
	// which has its own @sveltejs/kit under pnpm. Left external, its server
	// modules would load THAT copy (realpath resolution) and crash with
	// "Could not get the request store" — two kit instances, two
	// AsyncLocalStorage worlds. Bundling norns through Vite dedupes its kit
	// imports onto the app's single copy.
	// Accept reverse-proxied Host headers in dev (norns lint: vite/allowed-hosts).
	server: { allowedHosts: true },
	resolve: { dedupe: ['@sveltejs/kit'] },
	ssr: { noExternal: ['@human-synthesis/norns'] },
	// norns-ui ships .n (Pug + Civet) sources that only the norns plugins can
	// compile. Left to Vite's dependency optimizer, a re-optimization during
	// dev (a page importing a component for the first time) hands them to
	// esbuild and fails with "No loader is configured for .n files".
	optimizeDeps: { exclude: ['@human-synthesis/norns-ui'] }
});
