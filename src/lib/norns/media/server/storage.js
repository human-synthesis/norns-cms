// Media storage behind one interface: put(key, bytes, mime), get(key) →
// { body, mime, size } | null, delete(key).
//
//   r2Storage(bucket)  Cloudflare R2, the `MEDIA` binding (production, and
//                      `bun run dev` through the platform proxy)
//   dirStorage(dir)    a local directory (bun test, a Bun server without R2)
import { mkdirSync, writeFileSync, readFileSync, existsSync, rmSync, statSync } from 'node:fs';
import { dirname, join, normalize } from 'node:path';

export function r2Storage(bucket) {
	return {
		kind: 'r2',
		async put(key, bytes, mime) {
			await bucket.put(key, bytes, { httpMetadata: { contentType: mime, cacheControl: 'public, max-age=31536000, immutable' } });
		},
		async get(key) {
			const obj = await bucket.get(key);
			if (!obj) return null;
			return { body: obj.body, mime: obj.httpMetadata?.contentType ?? 'application/octet-stream', size: obj.size, etag: obj.httpEtag };
		},
		async delete(key) {
			await bucket.delete(key);
		}
	};
}

export function dirStorage(dir) {
	const pathOf = (key) => {
		const p = normalize(join(dir, key));
		if (!p.startsWith(normalize(dir))) throw new Error('media: bad key');
		return p;
	};
	return {
		kind: 'dir',
		async put(key, bytes) {
			const p = pathOf(key);
			mkdirSync(dirname(p), { recursive: true });
			writeFileSync(p, Buffer.from(bytes));
		},
		async get(key, mime) {
			const p = pathOf(key);
			if (!existsSync(p)) return null;
			return { body: readFileSync(p), mime: mime ?? 'application/octet-stream', size: statSync(p).size, etag: null };
		},
		async delete(key) {
			rmSync(pathOf(key), { force: true });
		}
	};
}

/** Width and height from PNG, GIF, JPEG and WebP headers (no decoding). */
export function imageSize(bytes) {
	const b = bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes);
	const u16 = (i) => (b[i] << 8) | b[i + 1];
	const u16le = (i) => b[i] | (b[i + 1] << 8);
	const u32 = (i) => ((b[i] << 24) | (b[i + 1] << 16) | (b[i + 2] << 8) | b[i + 3]) >>> 0;
	if (b.length > 24 && b[0] === 0x89 && b[1] === 0x50 && b[2] === 0x4e && b[3] === 0x47) return { width: u32(16), height: u32(20) };
	if (b.length > 10 && b[0] === 0x47 && b[1] === 0x49 && b[2] === 0x46) return { width: u16le(6), height: u16le(8) };
	if (b.length > 4 && b[0] === 0xff && b[1] === 0xd8) {
		let i = 2;
		while (i + 9 < b.length) {
			if (b[i] !== 0xff) {
				i++;
				continue;
			}
			const marker = b[i + 1];
			const len = u16(i + 2);
			if (marker >= 0xc0 && marker <= 0xcf && ![0xc4, 0xc8, 0xcc].includes(marker)) return { width: u16(i + 7), height: u16(i + 5) };
			i += 2 + len;
		}
		return null;
	}
	if (b.length > 30 && b[0] === 0x52 && b[1] === 0x49 && b[2] === 0x46 && b[3] === 0x46 && b[8] === 0x57 && b[9] === 0x45) {
		const chunk = String.fromCharCode(b[12], b[13], b[14], b[15]);
		if (chunk === 'VP8X') return { width: 1 + (b[24] | (b[25] << 8) | (b[26] << 16)), height: 1 + (b[27] | (b[28] << 8) | (b[29] << 16)) };
		if (chunk === 'VP8 ') return { width: u16le(26) & 0x3fff, height: u16le(28) & 0x3fff };
		if (chunk === 'VP8L') {
			const bits = b[21] | (b[22] << 8) | (b[23] << 16) | (b[24] << 24);
			return { width: (bits & 0x3fff) + 1, height: ((bits >> 14) & 0x3fff) + 1 };
		}
	}
	return null;
}

/** Sniff the real type from magic bytes; never trust the browser's claim. */
export function sniffMime(bytes) {
	const b = bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes);
	const at = (i, ...xs) => xs.every((x, k) => b[i + k] === x);
	if (at(0, 0x89, 0x50, 0x4e, 0x47)) return 'image/png';
	if (at(0, 0xff, 0xd8, 0xff)) return 'image/jpeg';
	if (at(0, 0x47, 0x49, 0x46, 0x38)) return 'image/gif';
	if (at(0, 0x52, 0x49, 0x46, 0x46) && at(8, 0x57, 0x45, 0x42, 0x50)) return 'image/webp';
	if (at(4, 0x66, 0x74, 0x79, 0x70, 0x61, 0x76, 0x69, 0x66)) return 'image/avif';
	if (at(0, 0x25, 0x50, 0x44, 0x46)) return 'application/pdf';
	return null;
}
