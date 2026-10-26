// Small string helpers for XML / text outputs.
export const xmlEscape = (s) =>
	String(s ?? '')
		.replace(/&/g, '&amp;')
		.replace(/</g, '&lt;')
		.replace(/>/g, '&gt;')
		.replace(/"/g, '&quot;')
		.replace(/'/g, '&apos;');

export const iso = (ms) => new Date(Number(ms) || Date.now()).toISOString();
export const rfc822 = (ms) => new Date(Number(ms) || Date.now()).toUTCString();

/** JSON for <script type="application/ld+json">, safe inside HTML. */
export const ldJson = (obj) => JSON.stringify(obj).replace(/</g, '\\u003c').replace(/>/g, '\\u003e').replace(/&/g, '\\u0026');

/** Markdown/HTML → plain text, for descriptions. */
export const plain = (s, max = 0) => {
	const t = String(s ?? '')
		.replace(/<\/(p|li|h[1-6]|div|tr|td|th|blockquote|figcaption)>|<br\s*\/?>/gi, ' ')
		.replace(/<[^>]+>/g, '')
		.replace(/```[\s\S]*?```/g, ' ')
		.replace(/^:{2,3}.*$/gm, ' ')
		.replace(/!\[[^\]]*\]\([^)]*\)/g, ' ')
		.replace(/\[([^\]]*)\]\([^)]*\)/g, '$1')
		.replace(/[#>*_`|~]/g, ' ')
		.replace(/\s+/g, ' ')
		.trim();
	if (!max || t.length <= max) return t;
	const cut = t.slice(0, max - 1);
	return cut.slice(0, cut.lastIndexOf(' ') > max * 0.6 ? cut.lastIndexOf(' ') : cut.length) + '…';
};
