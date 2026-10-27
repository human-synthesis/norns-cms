// Rich Markdown → sanitised HTML.
//
// Markdown is the storage format everywhere; "rich" comes from renderer
// extensions, not from HTML in the database:
//
//   GFM (tables, task lists, strikethrough), footnote-free
//   ```lang fences          highlighted with highlight.js (a fixed set of languages)
//   ![alt](media:12 "cap")  responsive image from the media library, as a <figure>
//   :::note|tip|info|warning|danger [title] … :::   callout boxes
//   ::youtube{id=…}  ::vimeo{id=…}  ::embed{url=https://… title=…}
//                           consent-aware embeds: a placeholder until the visitor
//                           allows marketing cookies or clicks "Load"
//   ::article{slug=…}       inline card linking another article
//   [[toc]]                 table of contents
//
// Raw HTML in the source is sanitised with an allowlist (js-xss); links and
// images with non-http(s)/relative URLs are dropped. Headings get stable ids
// so the TOC and assistants can deep-link.
//
// render(md, ctx) is synchronous; ctx carries everything looked up up front:
//   ctx.media    { [id]: { url, width, height, alt, caption, srcset } }
//   ctx.articles { [slug]: { title, url, summary } }
// collectRefs(md) tells the caller which ids/slugs to look up.
import { Marked } from 'marked';
import hljs from 'highlight.js/lib/core';
import javascript from 'highlight.js/lib/languages/javascript';
import typescript from 'highlight.js/lib/languages/typescript';
import json from 'highlight.js/lib/languages/json';
import bash from 'highlight.js/lib/languages/bash';
import css from 'highlight.js/lib/languages/css';
import xml from 'highlight.js/lib/languages/xml';
import python from 'highlight.js/lib/languages/python';
import sql from 'highlight.js/lib/languages/sql';
import yaml from 'highlight.js/lib/languages/yaml';
import markdown from 'highlight.js/lib/languages/markdown';
import diff from 'highlight.js/lib/languages/diff';
import go from 'highlight.js/lib/languages/go';
import rust from 'highlight.js/lib/languages/rust';
import php from 'highlight.js/lib/languages/php';
import xss from 'xss';

const LANGS = { javascript, typescript, json, bash, css, xml, python, sql, yaml, markdown, diff, go, rust, php };
for (const [name, lang] of Object.entries(LANGS)) hljs.registerLanguage(name, lang);
const ALIASES = { js: 'javascript', ts: 'typescript', sh: 'bash', shell: 'bash', zsh: 'bash', html: 'xml', svelte: 'xml', py: 'python', yml: 'yaml', md: 'markdown', civet: 'typescript' };

export const escapeHtml = (s) =>
	String(s ?? '')
		.replace(/&/g, '&amp;')
		.replace(/</g, '&lt;')
		.replace(/>/g, '&gt;')
		.replace(/"/g, '&quot;')
		.replace(/'/g, '&#39;');

export const slugifyHeading = (s) =>
	String(s)
		.toLowerCase()
		.normalize('NFKD')
		.replace(/[̀-ͯ]/g, '')
		.replace(/<[^>]+>/g, '')
		.replace(/&[a-z]+;/g, '')
		.replace(/[^a-z0-9]+/g, '-')
		.replace(/^-+|-+$/g, '') || 'section';

const safeUrl = (href) => {
	const h = String(href ?? '').trim();
	if (/^(https?:|mailto:|tel:)/i.test(h)) return h;
	if (/^[/#?.]/.test(h) && !h.startsWith('//')) return h;
	if (/^[\w-]+(\/|$)/.test(h) && !/^[\w+.-]+:/.test(h)) return h; // bare relative path
	return null;
};

const ALLOW = xss.getDefaultWhiteList();
for (const tag of ['figure', 'figcaption', 'aside', 'section', 'details', 'summary', 'mark', 'kbd', 'sup', 'sub', 'abbr', 'dl', 'dt', 'dd']) ALLOW[tag] = ALLOW[tag] ?? [];
for (const tag of Object.keys(ALLOW)) ALLOW[tag] = [...new Set([...(ALLOW[tag] ?? []), 'class', 'id', 'title'])];
const sanitizer = new xss.FilterXSS({
	whiteList: ALLOW,
	stripIgnoreTag: true,
	stripIgnoreTagBody: ['script', 'style', 'iframe', 'object', 'embed'],
	safeAttrValue: (tag, name, value, cssFilter) => {
		if (name === 'href' || name === 'src') return safeUrl(value) ? xss.escapeAttrValue(value) : '';
		return xss.safeAttrValue(tag, name, value, cssFilter);
	}
});
export const sanitizeHtml = (html) => sanitizer.process(String(html ?? ''));

const parseAttrs = (s) => {
	const out = {};
	const re = /([a-zA-Z][\w-]*)=(?:"([^"]*)"|'([^']*)'|([^\s}]+))/g;
	let m;
	while ((m = re.exec(s))) out[m[1]] = m[2] ?? m[3] ?? m[4] ?? '';
	return out;
};

const MEDIA_RE = /!\[[^\]]*\]\(\s*media:(\d+)/g;
const ARTICLE_RE = /^::article\{([^}\n]*)\}/gm;

/** Ids and slugs a document references, so the caller can look them up first. */
export function collectRefs(md) {
	const media = new Set();
	const articles = new Set();
	const src = String(md ?? '');
	let m;
	MEDIA_RE.lastIndex = 0;
	while ((m = MEDIA_RE.exec(src))) media.add(Number(m[1]));
	ARTICLE_RE.lastIndex = 0;
	while ((m = ARTICLE_RE.exec(src))) {
		const slug = parseAttrs(m[1]).slug;
		if (slug) articles.add(slug);
	}
	return { media: [...media], articles: [...articles] };
}

const PROVIDERS = {
	youtube: (a) => (/^[\w-]{6,20}$/.test(a.id ?? '') ? { src: `https://www.youtube-nocookie.com/embed/${a.id}`, label: 'YouTube' } : null),
	vimeo: (a) => (/^\d{4,15}$/.test(a.id ?? '') ? { src: `https://player.vimeo.com/video/${a.id}?dnt=1`, label: 'Vimeo' } : null),
	embed: (a) => {
		if (!/^https:\/\//i.test(a.url ?? '')) return null;
		let host = 'an external site';
		try {
			host = new URL(a.url).hostname;
		} catch {
			return null;
		}
		return { src: a.url, label: host };
	}
};

const fillText = (text, vars) => String(text).replace(/\{(\w+)\}/g, (m, k) => (vars[k] == null ? m : vars[k]));

/**
 * A consent-aware embed placeholder; the site's client script swaps in the
 * iframe. `strings` are the interface texts of the entry's language.
 */
export function embedHtml(kind, attrs, strings = {}) {
	const p = PROVIDERS[kind]?.(attrs);
	if (!p) return '';
	const host = p.label;
	const title = attrs.title || fillText(strings['embed.content'] ?? '{host} content', { host });
	const notice = fillText(strings['embed.notice'] ?? '{title} is hosted by {host}, which may set cookies.', { title: escapeHtml(title), host: escapeHtml(host) });
	const load = fillText(strings['embed.load'] ?? 'Load {host} content', { host: escapeHtml(host) });
	return (
		`<div class="embed" data-embed-src="${escapeHtml(p.src)}" data-embed-title="${escapeHtml(title)}" data-consent="marketing">` +
		`<div class="embed-placeholder"><p>${notice}</p>` +
		`<button type="button" class="btn btn-secondary btn-sm" data-embed-load>${load}</button></div></div>`
	);
}

const figure = (m, alt, caption) => {
	const altText = alt || m.alt || '';
	const cap = caption || m.caption || '';
	const dims = m.width && m.height ? ` width="${m.width}" height="${m.height}"` : '';
	const srcset = m.srcset ? ` srcset="${escapeHtml(m.srcset)}" sizes="(min-width: 768px) 720px, 100vw"` : '';
	return (
		`<figure class="md-figure"><img src="${escapeHtml(m.url)}" alt="${escapeHtml(altText)}"${dims}${srcset} loading="lazy" decoding="async">` +
		(cap ? `<figcaption>${escapeHtml(cap)}</figcaption>` : '') +
		`</figure>`
	);
};

/**
 * @param {string} md
 * @param {{ media?: Record<string, any>, articles?: Record<string, any> }} ctx
 * @returns {{ html: string, toc: { depth: number, text: string, id: string }[], words: number }}
 */
export function renderMarkdown(md, ctx = {}) {
	ctx.strings ??= {};
	const media = ctx.media ?? {};
	const articles = ctx.articles ?? {};
	const toc = [];
	const ids = new Map();
	const uniqueId = (base) => {
		const n = ids.get(base) ?? 0;
		ids.set(base, n + 1);
		return n ? `${base}-${n + 1}` : base;
	};

	const marked = new Marked({ gfm: true, breaks: false });
	marked.use({
		extensions: [
			{
				name: 'callout',
				level: 'block',
				start: (src) => src.match(/^:::/m)?.index,
				tokenizer(src) {
					const m = /^:::(note|tip|info|warning|danger)(?:[ \t]+([^\n]*))?\n([\s\S]*?)\n:::[ \t]*(?:\n|$)/.exec(src);
					if (!m) return undefined;
					const token = { type: 'callout', raw: m[0], kind: m[1], title: (m[2] ?? '').trim(), tokens: [] };
					this.lexer.blockTokens(m[3], token.tokens);
					return token;
				},
				renderer(t) {
					const title = t.title ? `<p class="callout-title">${escapeHtml(t.title)}</p>` : '';
					return `<aside class="callout callout-${t.kind}">${title}${this.parser.parse(t.tokens)}</aside>\n`;
				}
			},
			{
				name: 'directive',
				level: 'block',
				start: (src) => src.match(/^::[a-z]/m)?.index,
				tokenizer(src) {
					const m = /^::([a-z]+)\{([^}\n]*)\}[ \t]*(?:\n|$)/.exec(src);
					if (!m) return undefined;
					return { type: 'directive', raw: m[0], name: m[1], attrs: parseAttrs(m[2]) };
				},
				renderer(t) {
					if (t.name === 'article') {
						const a = articles[t.attrs.slug];
						if (!a) return '';
						const summary = a.summary ? `<span class="md-article-summary">${escapeHtml(a.summary)}</span>` : '';
						return `<a class="md-article-card" href="${escapeHtml(a.url)}"><strong>${escapeHtml(a.title)}</strong>${summary}</a>\n`;
					}
					return embedHtml(t.name, t.attrs, ctx.strings) + '\n';
				}
			},
			{
				name: 'toc',
				level: 'block',
				start: (src) => src.match(/^\[\[toc\]\]/m)?.index,
				tokenizer(src) {
					const m = /^\[\[toc\]\][ \t]*(?:\n|$)/.exec(src);
					return m ? { type: 'toc', raw: m[0] } : undefined;
				},
				renderer: () => '<!--cms-toc-->\n'
			}
		],
		renderer: {
			heading({ tokens, depth }) {
				const inner = this.parser.parseInline(tokens);
				const text = inner.replace(/<[^>]+>/g, '');
				const id = uniqueId(slugifyHeading(text));
				if (depth >= 2 && depth <= 3) toc.push({ depth, text: text.replace(/&amp;/g, '&').replace(/&#39;/g, "'").replace(/&quot;/g, '"'), id });
				return `<h${depth} id="${id}"><a class="heading-anchor" href="#${id}" aria-hidden="true" tabindex="-1">#</a>${inner}</h${depth}>\n`;
			},
			paragraph({ tokens }) {
				if (tokens.length === 1 && tokens[0].type === 'image') return this.parser.parseInline(tokens) + '\n';
				return `<p>${this.parser.parseInline(tokens)}</p>\n`;
			},
			image({ href, title, text }) {
				const m = /^media:(\d+)$/.exec(String(href ?? '').trim());
				if (m) {
					const item = media[m[1]];
					return item ? figure(item, text, title) : '';
				}
				const url = safeUrl(href);
				if (!url) return '';
				return `<img src="${escapeHtml(url)}" alt="${escapeHtml(text)}"${title ? ` title="${escapeHtml(title)}"` : ''} loading="lazy" decoding="async">`;
			},
			link({ href, title, tokens }) {
				const inner = this.parser.parseInline(tokens);
				const url = safeUrl(href);
				if (!url) return inner;
				const external = /^https?:\/\//i.test(url);
				return `<a href="${escapeHtml(url)}"${title ? ` title="${escapeHtml(title)}"` : ''}${external ? ' rel="noopener noreferrer"' : ''}>${inner}</a>`;
			},
			code({ text, lang }) {
				const name = String(lang ?? '').trim().split(/\s+/)[0].toLowerCase();
				const resolved = ALIASES[name] ?? name;
				let body;
				if (resolved && hljs.getLanguage(resolved)) body = hljs.highlight(text, { language: resolved, ignoreIllegals: true }).value;
				else body = escapeHtml(text);
				const label = name ? `<span class="code-lang">${escapeHtml(name)}</span>` : '';
				return `<div class="code-block">${label}<pre><code class="hljs${resolved ? ` language-${escapeHtml(resolved)}` : ''}">${body}</code></pre></div>\n`;
			},
			html({ text }) {
				return sanitizeHtml(text);
			},
			table(token) {
				// Wrap tables so wide ones scroll instead of breaking the layout.
				let header = '';
				for (const cell of token.header) header += this.tablecell(cell);
				let body = '';
				for (const row of token.rows) {
					let r = '';
					for (const cell of row) r += this.tablecell(cell);
					body += `<tr>${r}</tr>\n`;
				}
				return `<div class="table-wrap"><table><thead><tr>${header}</tr></thead>${body ? `<tbody>${body}</tbody>` : ''}</table></div>\n`;
			}
		}
	});

	let html = marked.parse(String(md ?? ''), { async: false });
	if (html.includes('<!--cms-toc-->')) {
		const items = toc.map((t) => `<li class="toc-${t.depth}"><a href="#${t.id}">${escapeHtml(t.text)}</a></li>`).join('');
		html = html.replace(/<!--cms-toc-->/g, items ? `<nav class="toc" aria-label="Table of contents"><p class="toc-title">Contents</p><ol>${items}</ol></nav>` : '');
	}
	const prose = String(md ?? '')
		.replace(/```[\s\S]*?```/g, ' ')
		.replace(/^::.*$/gm, ' ')
		.replace(/[#>*_`[\]()!|-]/g, ' ');
	const words = (prose.match(/[\p{L}\p{N}]+/gu) ?? []).length;
	return { html, toc, words };
}
