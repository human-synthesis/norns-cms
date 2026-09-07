// Bun plugin that lets `bun test` import Norns `.c` / `.civet` modules.
//
// The Vite pipeline (nornsCivetPlugin) does this for dev/build; tests run
// outside Vite, so we compile with Civet here and point extension-less
// relative imports (`import { X } from './repo'`) at their `.c` file.
// Registered via bunfig.toml `[test] preload`.
import { plugin } from 'bun';
import { existsSync } from 'node:fs';
import { dirname, extname, resolve } from 'node:path';
import { compile } from '@danielx/civet';

const CIVET_EXTS = ['.c', '.civet'];

/** `./repo` → `./repo.c` when that file exists next to the importer. */
function withCivetExt(spec, importerDir) {
	if (!/^\.{1,2}\//.test(spec) || extname(spec)) return spec;
	const base = resolve(importerDir, spec);
	for (const ext of CIVET_EXTS) {
		if (existsSync(base + ext)) return spec + ext;
	}
	return spec;
}

plugin({
	name: 'norns-civet',
	setup(build) {
		build.onLoad({ filter: /\.(c|civet)$/ }, async ({ path }) => {
			const source = await Bun.file(path).text();
			const result = await compile(source, { js: true, filename: path });
			let code = typeof result === 'string' ? result : result.code;
			// Bun resolves the compiled module's imports itself and does not
			// try `.c`; rewrite the specifiers in the emitted JS instead.
			const dir = dirname(path);
			code = code.replace(
				/(\bfrom\s*|\bimport\s*\(?\s*)(['"])(\.{1,2}\/[^'"]+)\2/g,
				(m, lead, q, spec) => `${lead}${q}${withCivetExt(spec, dir)}${q}`
			);
			return { contents: code, loader: 'js' };
		});
	}
});
