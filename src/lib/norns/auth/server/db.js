// One async SQL surface over two backends, so repos never know where they
// run:
//
//   sqliteDb(path)  — bun:sqlite under Bun, better-sqlite3 under Node. Local
//                     dev without a D1 binding, `bun test`, tools/seed.js.
//   d1Db(binding)   — Cloudflare D1, from event.platform.env.DB (wrangler in
//                     production, adapter-cloudflare's platform proxy in dev).
//
// Surface: all(sql, params) → rows, get(sql, params) → row | undefined,
// run(sql, params) → { lastInsertRowid, changes }, exec(sql). Always await.
//
// Plain JS on purpose: createRequire + a runtime switch read better here
// than in Civet.
import { createRequire } from 'node:module';
import { mkdirSync } from 'node:fs';
import { dirname } from 'node:path';

/** DATABASE_URL (file:...) → path. Defaults to data/app.db, like `norns migrate`. */
export function resolveDbPath() {
	const url = process.env.DATABASE_URL || 'file:data/app.db';
	if (!url.startsWith('file:')) {
		throw new Error(`norns-cms: only SQLite (file:...) is supported locally; got "${url}"`);
	}
	return url.slice(5);
}

/** @param {string} path SQLite file path, or ':memory:' */
export function sqliteDb(path) {
	if (path !== ':memory:') mkdirSync(dirname(path), { recursive: true });
	const require = createRequire(import.meta.url);
	let raw;
	if (typeof Bun !== 'undefined') {
		const { Database } = require('bun:sqlite');
		raw = new Database(path);
		raw.pragma = (stmt) => raw.exec('PRAGMA ' + stmt);
	} else {
		let Database;
		try {
			Database = require('better-sqlite3');
		} catch {
			throw new Error('norns-cms: running under Node needs better-sqlite3. Run: bun add better-sqlite3');
		}
		raw = new Database(path);
	}
	if (path !== ':memory:') raw.pragma('journal_mode = WAL');
	raw.pragma('foreign_keys = ON');
	return {
		kind: 'sqlite',
		raw,
		async all(sql, params = []) {
			return raw.prepare(sql).all(...params);
		},
		async get(sql, params = []) {
			// bun:sqlite returns null for "no row"; normalise to undefined.
			return raw.prepare(sql).get(...params) ?? undefined;
		},
		async run(sql, params = []) {
			const r = raw.prepare(sql).run(...params);
			return { lastInsertRowid: Number(r.lastInsertRowid), changes: Number(r.changes) };
		},
		async exec(sql) {
			raw.exec(sql);
		}
	};
}

/** @param {any} binding a D1Database */
export function d1Db(binding) {
	return {
		kind: 'd1',
		raw: binding,
		async all(sql, params = []) {
			const r = await binding.prepare(sql).bind(...params).all();
			return r.results;
		},
		async get(sql, params = []) {
			const row = await binding.prepare(sql).bind(...params).first();
			return row ?? undefined;
		},
		async run(sql, params = []) {
			const r = await binding.prepare(sql).bind(...params).run();
			return { lastInsertRowid: Number(r.meta.last_row_id), changes: Number(r.meta.changes) };
		},
		async exec(sql) {
			await binding.exec(sql);
		}
	};
}
