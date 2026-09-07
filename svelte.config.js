import adapter from '@sveltejs/adapter-auto';
import { nornsAutoImport } from '@human-synthesis/norns/auto-import';
import { nornsConfig } from '@human-synthesis/norns/config';
import { nornsPreprocess } from '@human-synthesis/norns/preprocess';
import { presetUI } from '@human-synthesis/norns-ui/auto-import';

const ui = presetUI();

// The same nornsAutoImport instance is registered here (Svelte preprocessor
// hooks, for .n / .svelte files) and in vite.config.js (Vite plugin hooks,
// for standalone .c modules). Keep the two option objects identical.
export default nornsConfig({
	preprocess: [
		...nornsPreprocess(),
		nornsAutoImport({
			componentDirs: ['src/lib/components', 'src/routes'],
			components: ui.components
		})
	],
	kit: {
		adapter: adapter()
	}
});
