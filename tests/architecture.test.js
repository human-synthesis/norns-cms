// Boundary rules for feature folders, checked by scanning import lines.
//
//  1. Routes and hooks import a feature only through its server/public,
//     shared/, or ui/ files — never service.c, repo.c, or module.c.
//  2. A feature reaches another feature only through server/public.
//  3. shared/ and ui/ never import from server/.
//
// Regex-level on purpose; norns check and the runtime carry the real weight.
import { describe, test, expect } from 'bun:test';
import { readdirSync, readFileSync, statSync } from 'node:fs';
import { join, relative } from 'node:path';

const SRC = join(import.meta.dir, '..', 'src');

function walk(dir, out = []) {
	for (const name of readdirSync(dir)) {
		const full = join(dir, name);
		if (statSync(full).isDirectory()) walk(full, out);
		else if (/\.(c|civet|n|js|ts)$/.test(name)) out.push(full);
	}
	return out;
}

function imports(file) {
	const src = readFileSync(file, 'utf8');
	const out = [];
	const re = /^\s*import\b[^'"]*['"]([^'"]+)['"]/gm;
	let m;
	while ((m = re.exec(src))) out.push(m[1]);
	return out;
}

/** `$lib/norns/<f>/...` or a relative path into src/lib/norns → { feature, rest } */
function featureOf(spec, fromFile) {
	let rel;
	if (spec.startsWith('$lib/norns/')) rel = spec.slice('$lib/norns/'.length);
	else if (spec.startsWith('.')) {
		const abs = join(fromFile, '..', spec);
		const r = relative(join(SRC, 'lib', 'norns'), abs);
		if (r.startsWith('..')) return null;
		rel = r;
	} else return null;
	const [feature, ...rest] = rel.split('/');
	return { feature, rest: rest.join('/') };
}

function violations() {
	const found = [];
	for (const file of walk(SRC)) {
		const here = relative(SRC, file);
		const own = here.startsWith('lib/norns/') ? here.split('/')[2] : null;
		const section = own ? here.split('/')[3] : null; // server | shared | ui
		for (const spec of imports(file)) {
			const target = featureOf(spec, file);
			if (!target) continue;
			const crossFeature = target.feature !== own;
			const intoServer = target.rest.startsWith('server/');
			const isPublic = /^server\/public(\.c)?$/.test(target.rest);
			if (crossFeature && intoServer && !isPublic) {
				found.push(`${here} → ${spec} (only server/public may be imported across features)`);
			}
			if (!crossFeature && (section === 'shared' || section === 'ui') && intoServer) {
				found.push(`${here} → ${spec} (${section}/ must not import server/)`);
			}
		}
	}
	return found;
}

describe('architecture', () => {
	test('feature boundaries hold', () => {
		expect(violations()).toEqual([]);
	});

	test('exactly one module owns the db binding', () => {
		const owners = walk(join(SRC, 'lib', 'norns')).filter(
			(f) => f.endsWith('module.c') && /(single|bind)\s+'db'/.test(readFileSync(f, 'utf8'))
		);
		expect(owners.map((f) => relative(SRC, f))).toEqual(['lib/norns/auth/server/module.c']);
	});
});
