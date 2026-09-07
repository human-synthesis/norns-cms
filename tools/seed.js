// Create the first admin user. Reads CMS_ADMIN_EMAIL / CMS_ADMIN_PASSWORD
// from the environment (preferred) or from argv. Idempotent: an existing
// email is left untouched. Run `bun run migrate up` first.
import { openSqlite, resolveDbPath } from '../src/lib/norns/auth/server/sqlite.js';
import { hashPassword } from '../src/lib/norns/auth/server/password.js';

const email = (process.env.CMS_ADMIN_EMAIL || process.argv[2] || '').trim().toLowerCase();
const password = process.env.CMS_ADMIN_PASSWORD || process.argv[3] || '';
const name = process.env.CMS_ADMIN_NAME || process.argv[4] || 'Admin';

if (!email || !password) {
	console.error(
		'Usage: CMS_ADMIN_EMAIL=you@example.com CMS_ADMIN_PASSWORD=... bun run cms:seed\n' +
			'   or: bun run cms:seed <email> <password> [name]'
	);
	process.exit(1);
}

const db = openSqlite(resolveDbPath());

let hasUsers;
try {
	hasUsers = db.prepare('SELECT COUNT(*) AS n FROM users').get();
} catch {
	console.error('The users table does not exist yet. Run: bun run migrate up');
	process.exit(1);
}

const existing = db.prepare('SELECT id FROM users WHERE email = ?').get(email);
if (existing) {
	console.log(`User ${email} already exists (id ${existing.id}); nothing to do.`);
	process.exit(0);
}

const role = hasUsers.n === 0 ? 'admin' : 'editor';
db.prepare('INSERT INTO users (email, name, password_hash, role, created_at) VALUES (?, ?, ?, ?, ?)').run(
	email,
	name,
	hashPassword(password),
	role,
	Date.now()
);
console.log(`Created ${role} ${email}. Sign in at /login.`);
