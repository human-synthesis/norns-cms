// SQLite opener shared by the runtime (auth/server/module.c owns the `db`
// token) and by tools/seed.js. Plain JS on purpose: it needs createRequire
// and a runtime switch, which read better here than in Civet.
//
// Under Bun, `bun:sqlite` (built in, no native build). Under Node,
// `better-sqlite3` (install it yourself: `bun add better-sqlite3`). Both
// expose the same prepare().all/get/run surface the repos use; bun:sqlite
// lacks `pragma()`, so a shim is grafted on.
import { createRequire } from 'node:module';
import { mkdirSync } from 'node:fs';
import { dirname } from 'node:path';

/** DATABASE_URL (file:...) → path. Defaults to data/app.db, like `norns migrate`. */
export function resolveDbPath() {
	const url = process.env.DATABASE_URL || 'file:data/app.db';
	if (!url.startsWith('file:')) {
		throw new Error(`norns-cms: only SQLite (file:...) is supported in this version; got "${url}"`);
	}
	return url.slice(5);
}

/** @param {string} path SQLite file path, or ':memory:' */
export function openSqlite(path) {
	if (path !== ':memory:') mkdirSync(dirname(path), { recursive: true });
	const require = createRequire(import.meta.url);
	let db;
	if (typeof Bun !== 'undefined') {
		const { Database } = require('bun:sqlite');
		db = new Database(path);
		db.pragma = (stmt) => db.exec('PRAGMA ' + stmt);
	} else {
		let Database;
		try {
			Database = require('better-sqlite3');
		} catch {
			throw new Error('norns-cms: running under Node needs better-sqlite3. Run: bun add better-sqlite3');
		}
		db = new Database(path);
	}
	if (path !== ':memory:') db.pragma('journal_mode = WAL');
	db.pragma('foreign_keys = ON');
	return db;
}
