// Recursive walks over field definitions and values: markdown sources for
// reference collection, rendering, and media ids for presentation. Plain JS
// because the recursion through blocks and repeaters reads better here.

/**
 * Visit every value of `fields` in `data`, descending into repeaters and
 * blocks. visit(field, value, path) is called for leaf fields.
 */
export function walkFields(fields, data, blockLookup, visit, path = []) {
	for (const f of fields ?? []) {
		const value = data?.[f.name];
		if (f.type === 'repeater') {
			(Array.isArray(value) ? value : []).forEach((item, i) => walkFields(f.fields, item, blockLookup, visit, [...path, f.name, i]));
		} else if (f.type === 'blocks') {
			(Array.isArray(value) ? value : []).forEach((block, i) => {
				const def = blockLookup(block?.type);
				if (def) walkFields(def.fields, block.props, blockLookup, visit, [...path, f.name, i, 'props']);
			});
		} else visit(f, value, path);
	}
}

/** Every markdown source string in a document. */
export function markdownSources(fields, data, blockLookup) {
	const out = [];
	walkFields(fields, data, blockLookup, (f, value) => {
		if (f.type === 'markdown' && value) out.push(String(value));
	});
	return out;
}

/** Every media id referenced by image fields (not markdown; that is pre-rendered). */
export function mediaIds(fields, data, blockLookup) {
	const out = new Set();
	walkFields(fields, data, blockLookup, (f, value) => {
		if (f.type === 'image' && Number(value) > 0) out.add(Number(value));
	});
	return [...out];
}

/**
 * Render markdown fields into a parallel structure:
 *   markdown field   → rendered[name] = { html, toc, words }
 *   repeater field   → rendered[name] = [ { …same shape per item } ]
 *   blocks field     → rendered[name] = [ { id, …same shape for block props } ]
 */
export function renderFields(fields, data, blockLookup, render) {
	const out = {};
	for (const f of fields ?? []) {
		const value = data?.[f.name];
		if (f.type === 'markdown') out[f.name] = render(String(value ?? ''));
		else if (f.type === 'repeater') {
			out[f.name] = (Array.isArray(value) ? value : []).map((item) => renderFields(f.fields, item, blockLookup, render));
		} else if (f.type === 'blocks') {
			out[f.name] = (Array.isArray(value) ? value : []).map((block) => {
				const def = blockLookup(block?.type);
				return { id: block?.id, ...(def ? renderFields(def.fields, block.props, blockLookup, render) : {}) };
			});
		}
	}
	return out;
}

/** FormData → object; keys that appear more than once become arrays. Files are skipped. */
export function formToObject(fd) {
	const out = {};
	for (const [key, value] of fd.entries()) {
		if (typeof value !== 'string') continue;
		if (key in out) out[key] = Array.isArray(out[key]) ? [...out[key], value] : [out[key], value];
		else out[key] = value;
	}
	return out;
}
