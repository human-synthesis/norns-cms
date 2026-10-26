// Create the first admin user.
//
//   bun run cms:seed            → the local SQLite file (DATABASE_URL)
//   bun run db:seed             → the local D1 (wrangler --local)
//   bun run db:seed:remote      → the production D1 (wrangler --remote)
//
// Reads CMS_ADMIN_EMAIL / CMS_ADMIN_PASSWORD / CMS_ADMIN_NAME from the
// environment (preferred) or from argv. Idempotent: an existing email is left
// untouched. Run the matching migrate command first.
import { writeFileSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { sqliteDb, resolveDbPath } from '../src/lib/norns/auth/server/db.js';
import { hashPassword } from '../src/lib/norns/auth/server/password.js';

const args = process.argv.slice(2);
const flags = new Set(args.filter((a) => a.startsWith('--')));
const positional = args.filter((a) => !a.startsWith('--'));

const email = (process.env.CMS_ADMIN_EMAIL || positional[0] || '').trim().toLowerCase();
const password = process.env.CMS_ADMIN_PASSWORD || positional[1] || '';
const name = process.env.CMS_ADMIN_NAME || positional[2] || 'Admin';
const D1_NAME = process.env.CMS_D1_NAME || 'norns-cms';

if (!email || !password) {
	console.error(
		'Usage: CMS_ADMIN_EMAIL=you@example.com CMS_ADMIN_PASSWORD=... bun run cms:seed|db:seed|db:seed:remote\n' +
			'   or: bun tools/seed.js [--d1 [--remote]] <email> <password> [name]'
	);
	process.exit(1);
}

const hash = hashPassword(password);
const slugOf = (s) => String(s).toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-+|-+$/g, '') || 'owner';

if (flags.has('--d1')) {
	// D1 has no local file API from here; hand a one-off SQL file to wrangler.
	// INSERT OR IGNORE keeps it idempotent on the UNIQUE email.
	const q = (s) => `'${String(s).replace(/'/g, "''")}'`;
	const sql =
		`INSERT OR IGNORE INTO users (email, name, password_hash, role, kind, status, slug, email_verified_at, created_at) ` +
		`VALUES (${q(email)}, ${q(name)}, ${q(hash)}, 'owner', 'staff', 'active', ${q(slugOf(name))}, ${Date.now()}, ${Date.now()});\n`;
	const file = join(tmpdir(), `norns-cms-seed-${process.pid}.sql`);
	writeFileSync(file, sql, { mode: 0o600 });
	try {
		const target = flags.has('--remote') ? '--remote' : '--local';
		const proc = Bun.spawnSync(['bunx', 'wrangler', 'd1', 'execute', D1_NAME, target, '--file', file], {
			stdout: 'inherit',
			stderr: 'inherit'
		});
		if (proc.exitCode !== 0) process.exit(proc.exitCode ?? 1);
	} finally {
		rmSync(file, { force: true });
	}
	console.log(`Seeded admin ${email} on D1 (${D1_NAME}, ${flags.has('--remote') ? 'remote' : 'local'}). Sign in at /login.`);
	process.exit(0);
}

const db = sqliteDb(resolveDbPath());

let count;
try {
	count = await db.get('SELECT COUNT(*) AS n FROM users', []);
} catch {
	console.error('The users table does not exist yet. Run: bun run migrate up');
	process.exit(1);
}

const existing = await db.get('SELECT id FROM users WHERE email = ?', [email]);
if (existing) {
	console.log(`User ${email} already exists (id ${existing.id}); nothing to do.`);
	process.exit(0);
}

// The first account owns the site; later ones made with this tool are editors.
const role = Number(count?.n ?? 0) === 0 ? 'owner' : 'editor';
await db.run(
	'INSERT INTO users (email, name, password_hash, role, kind, status, slug, email_verified_at, created_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)',
	[email, name, hash, role, 'staff', 'active', slugOf(name), Date.now(), Date.now()]
);
console.log(`Created ${role} ${email}. Sign in at /login.`);
