// Blocked paths: patterns, matching and probe detection. Pure, shared by
// the request handle, the admin screens and the tests.
//
// A pattern is a path where `*` matches anything (also `/`), compared
// case-insensitively against the whole path:
//   /wp-login.php   exactly that path
//   /wp-admin*      the path and everything under it
//   *.php           any path ending in .php

/** Paths the site itself needs: a pattern matching one of them is refused. */
export const PROTECTED = [
	'/',
	'/admin',
	'/admin/security/paths',
	'/api/v1/content',
	'/api/cron',
	'/login',
	'/account',
	'/account/login',
	'/_app/immutable/entry/start.js',
	'/media/1/image.jpg',
	'/forms/contact',
	'/comments',
	'/robots.txt',
	'/sitemap.xml',
	'/sitemap-article.xml',
	'/feed.xml',
	'/llms.txt',
	'/llms-full.txt',
	'/favicon.ico',
	'/favicon.svg',
	'/0123456789abcdef0123456789abcdef.txt', // IndexNow key file
	'/articles/a-post.md',
	'/.well-known/security.txt',
	'/.well-known/acme-challenge/token'
];

/** The starting list (source 'default'): what scanners ask for on any site. */
export const DEFAULT_PATTERNS = [
	['*.php', 'PHP: this site runs none'],
	['*.php5', 'PHP'],
	['*.phtml', 'PHP'],
	['*.asp', 'ASP'],
	['*.aspx', 'ASP.NET'],
	['*.jsp', 'Java pages'],
	['*.cgi', 'CGI scripts'],
	['/cgi-bin*', 'CGI scripts'],
	['/wp-admin*', 'WordPress'],
	['/wp-content*', 'WordPress'],
	['/wp-includes*', 'WordPress'],
	['/wp-json*', 'WordPress'],
	['/wordpress*', 'WordPress'],
	['/xmlrpc*', 'WordPress XML-RPC'],
	['/.env*', 'Environment files with secrets'],
	['*/.env', 'Environment files with secrets'],
	['/.git*', 'Git repository'],
	['/.svn*', 'Subversion repository'],
	['/.hg*', 'Mercurial repository'],
	['/.aws*', 'Cloud credentials'],
	['/.ssh*', 'SSH keys'],
	['/.docker*', 'Docker config'],
	['/.vscode*', 'Editor config'],
	['/.idea*', 'Editor config'],
	['/.ds_store', 'macOS folder file'],
	['/phpmyadmin*', 'Database admin'],
	['/pma*', 'Database admin'],
	['/adminer*', 'Database admin'],
	['/vendor/*', 'PHP packages (PHPUnit exploit)'],
	['/actuator*', 'Spring Boot'],
	['/server-status*', 'Apache status'],
	['/boaform*', 'Router exploit'],
	['/hnap1*', 'Router exploit'],
	['*.sql', 'Database dumps'],
	['*.sql.gz', 'Database dumps'],
	['*.bak', 'Backup files'],
	['*.swp', 'Editor swap files'],
	['/backup*', 'Backups'],
	['/config.json', 'Config probing'],
	['/docker-compose*', 'Docker config'],
	['/web.config', 'IIS config'],
	['/owa/*', 'Outlook Web Access'],
	['/autodiscover/*', 'Exchange autodiscover'],
	['/ecp/*', 'Exchange control panel']
];

/** Clean a typed pattern: one leading slash (unless it starts with *), lower case, no query. */
export function normalizePattern(input) {
	let p = String(input ?? '').trim().toLowerCase();
	try {
		if (/^https?:\/\//.test(p)) p = new URL(p).pathname.toLowerCase();
	} catch {
		/* keep as typed */
	}
	p = p.split('?')[0].split('#')[0].replace(/\*{2,}/g, '*');
	if (p && !p.startsWith('/') && !p.startsWith('*')) p = '/' + p;
	return p;
}

export function patternRegex(pattern) {
	const body = normalizePattern(pattern)
		.split('*')
		.map((s) => s.replace(/[.+?^${}()|[\]\\]/g, '\\$&'))
		.join('.*');
	return new RegExp(`^${body}$`, 'i');
}

export const matches = (pattern, path) => patternRegex(pattern).test(String(path ?? '').toLowerCase());

/** Why a pattern cannot be used, or '' when it can. */
export function patternProblem(pattern) {
	const p = normalizePattern(pattern);
	if (!p || p === '/' || p === '*' || /^\/?\*$/.test(p)) return 'Enter a path such as /wp-login.php, /wp-admin* or *.php';
	if (p.length > 200) return 'Keep the pattern under 200 characters';
	const re = patternRegex(p);
	const hit = PROTECTED.find((x) => re.test(x));
	if (hit) return `That would also block ${hit}, which the site needs`;
	return '';
}

const PROBE = [
	/\.(php\d?|phtml|asp|aspx|jsp|cgi|pl|env|ini|sql|bak|old|orig|swp|save|log|yml|yaml|conf|cfg|dist|tar|gz|zip|rar|7z)$/i,
	/(^|\/)\.(git|svn|hg|env|aws|ssh|docker|vscode|idea|htaccess|htpasswd|npmrc|DS_Store)/i,
	/(^|\/)(wp-|wordpress|xmlrpc|phpmyadmin|pma|adminer|phpunit|cgi-bin|actuator|boaform|hnap1|owa|ecp|autodiscover|solr|jenkins|manager\/html|console|telescope|_profiler|debug|setup|install|shell|cmd|eval)/i,
	/(\.\.\/|%2e%2e|\/etc\/passwd|\/proc\/self|<script|%3cscript|\$\{jndi)/i
];

/** Does a 404 path look like a vulnerability scan rather than a broken link? */
export const looksLikeProbe = (path) => PROBE.some((re) => re.test(String(path ?? '')));

/** A pattern to suggest for a probed path: the extension, the folder, or the path itself. */
export function suggestPattern(path) {
	const p = normalizePattern(path);
	const ext = /\.(php\d?|phtml|asp|aspx|jsp|cgi|sql|bak|swp)$/i.exec(p);
	if (ext) return `*.${ext[1].toLowerCase()}`;
	// A hidden file or folder anywhere (/.git, /en/.npmrc): every copy of it.
	const dot = /(?:^|\/)(\.(?!well-known)[a-z0-9_-]+)/i.exec(p);
	if (dot) return `*/${dot[1].toLowerCase()}*`;
	const first = /^\/([^/]+)\/./.exec(p);
	if (first && /^(wp-|wordpress|phpmyadmin|pma|adminer|cgi-bin|actuator|vendor|owa|ecp|autodiscover|solr|jenkins)/i.test(first[1])) return `/${first[1]}*`;
	return p;
}
