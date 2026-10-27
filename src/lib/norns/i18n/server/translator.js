// Machine translation with Claude, for drafts a person then reviews.
//
// Only the translatable strings of an entry (text, textarea, markdown,
// lines, tags, and the same inside blocks and repeaters) are sent, as a flat
// { path: text } object; Claude returns an object with exactly the same keys
// (structured output), which is merged back into a copy of the data. Media
// ids, URLs, slugs, selects and numbers never leave the server.
//
// Needs ANTHROPIC_API_KEY (a Worker secret). Model: ANTHROPIC_MODEL, default
// claude-opus-5-5. A refusal or a missing key is reported, never guessed.
import Anthropic from '@anthropic-ai/sdk';

const TEXT_TYPES = new Set(['text', 'textarea', 'markdown', 'lines', 'tags']);
const SKIP_FIELDS = new Set(['seoCanonical', 'navOrder']);

/** Flatten the translatable strings of `data` into { 'a.b.0.c': text }. */
export function collectStrings(fields, data, blockLookup, prefix = '', out = {}) {
	for (const f of fields ?? []) {
		if (SKIP_FIELDS.has(f.name)) continue;
		const value = data?.[f.name];
		const key = prefix + f.name;
		if (f.type === 'repeater') {
			(Array.isArray(value) ? value : []).forEach((item, i) => collectStrings(f.fields, item, blockLookup, `${key}.${i}.`, out));
		} else if (f.type === 'blocks') {
			(Array.isArray(value) ? value : []).forEach((block, i) => {
				const def = blockLookup(block?.type);
				if (def) collectStrings(def.fields, block.props, blockLookup, `${key}.${i}.props.`, out);
			});
		} else if (TEXT_TYPES.has(f.type)) {
			if (Array.isArray(value)) value.forEach((v, i) => typeof v === 'string' && v.trim() && (out[`${key}.${i}`] = v));
			else if (typeof value === 'string' && value.trim()) out[key] = value;
		}
	}
	return out;
}

/** Write translated strings back into a deep copy of `data`. */
export function applyStrings(data, strings) {
	const copy = JSON.parse(JSON.stringify(data ?? {}));
	for (const [path, text] of Object.entries(strings)) {
		const parts = path.split('.');
		let node = copy;
		for (let i = 0; i < parts.length - 1; i++) {
			if (node[parts[i]] == null) break;
			node = node[parts[i]];
		}
		const last = parts[parts.length - 1];
		if (node && typeof node === 'object' && last in node && typeof text === 'string') node[last] = text;
	}
	return copy;
}

const SYSTEM = `You translate website content for a content management system.

You receive a JSON object whose values are texts in the source language and return a JSON object with exactly the same keys, each value translated into the target language.

Keep everything that is not prose exactly as it is:
- Markdown structure: headings, lists, tables, emphasis, links (translate the link text, keep the URL), images (translate the alt text and title, keep the target, e.g. media:12).
- CMS directives: lines such as :::note Title, :::, ::youtube{id=…}, ::embed{url=…}, ::article{slug=…} and [[toc]]. Translate only the title after :::note / :::tip / :::warning / :::info / :::danger.
- Code (inline \`code\` and fenced blocks), URLs, e-mail addresses, product and brand names, and placeholders in braces such as {name}.

Write natural, idiomatic text for readers in the target language and region, as a professional translator would, not a word-for-word rendering. Match the tone of the source. Keep tag lists short: translate each tag as a short label.`;

/**
 * @param {Record<string, string>} strings
 * @param {{ from: string, to: string, apiKey?: string, model?: string, context?: string }} opts
 * @returns {Promise<Record<string, string>>}
 */
export async function translateStrings(strings, opts) {
	const keys = Object.keys(strings);
	if (!keys.length) return {};
	if (!opts.apiKey) throw new Error('Set the ANTHROPIC_API_KEY secret to translate with AI');
	const client = new Anthropic({ apiKey: opts.apiKey });
	const schema = {
		type: 'object',
		properties: Object.fromEntries(keys.map((k) => [k, { type: 'string' }])),
		required: keys,
		additionalProperties: false
	};
	const prompt =
		`Source language: ${opts.from}\nTarget language: ${opts.to}\n` +
		(opts.context ? `Context: ${opts.context}\n` : '') +
		`\nTranslate the values of this JSON object:\n\n${JSON.stringify(strings, null, 2)}`;

	// Streaming: long articles can produce long outputs. Server-side fallbacks
	// re-run a declined request on a suitable model within the same call.
	const stream = client.beta.messages.stream({
		model: opts.model || 'claude-opus-5-5',
		max_tokens: 64000,
		betas: ['server-side-fallback-2026-07-01'],
		fallbacks: 'default',
		output_config: { effort: 'medium', format: { type: 'json_schema', schema } },
		system: SYSTEM,
		messages: [{ role: 'user', content: prompt }]
	});
	const message = await stream.finalMessage();
	if (message.stop_reason === 'refusal') throw new Error('The model declined to translate this content');
	if (message.stop_reason === 'max_tokens') throw new Error('The translation was too long; translate it in parts');
	const text = message.content.filter((b) => b.type === 'text').map((b) => b.text).join('');
	let out;
	try {
		out = JSON.parse(text);
	} catch {
		throw new Error('The translation came back malformed; try again');
	}
	const result = {};
	for (const k of keys) if (typeof out[k] === 'string') result[k] = out[k];
	return result;
}
