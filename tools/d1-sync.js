// Mirror migrations/<feature>/<ts>_<slug>.sql into the flat migrations/d1/
// folder wrangler reads (`migrations_dir` accepts one directory). The mirror
// is committed; run this after adding a migration and commit both.
//
//   migrations/auth/20260907_001_init.sql → migrations/d1/20260907_001_auth_init.sql
import { readdirSync, readFileSync, writeFileSync, mkdirSync, statSync } from 'node:fs';
import { join } from 'node:path';

const root = join(import.meta.dir, '..', 'migrations');
const out = join(root, 'd1');
mkdirSync(out, { recursive: true });

let written = 0;
for (const feature of readdirSync(root).sort()) {
	if (feature === 'd1' || !statSync(join(root, feature)).isDirectory()) continue;
	for (const file of readdirSync(join(root, feature)).sort()) {
		if (!file.endsWith('.sql')) continue;
		const m = /^(\d{8}_\d{3})_(.+)\.sql$/.exec(file);
		const target = m ? `${m[1]}_${feature}_${m[2]}.sql` : `${feature}_${file}`;
		const src = readFileSync(join(root, feature, file), 'utf8');
		const header = `-- mirrored from migrations/${feature}/${file} by tools/d1-sync.js; edit the source, not this copy\n`;
		writeFileSync(join(out, target), header + src);
		written++;
	}
}
console.log(`d1-sync: ${written} file(s) in migrations/d1/`);
