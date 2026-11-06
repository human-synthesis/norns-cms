-- mirrored from migrations/protection/20261103_001_blocked_paths.sql by tools/d1-sync.js; edit the source, not this copy
-- Blocked paths: requests for these get an empty 403 before anything else
-- runs (protection/server/paths.c). The starting list below is DEFAULT_PATTERNS
-- in protection/shared/paths.js ("Add the starting list again" re-adds it);
-- people add more (source 'manual', or '404' when moved from the 404 log).
CREATE TABLE IF NOT EXISTS blocked_paths (
	id          INTEGER PRIMARY KEY AUTOINCREMENT,
	pattern     TEXT    NOT NULL UNIQUE,
	note        TEXT    NOT NULL DEFAULT '',
	source      TEXT    NOT NULL DEFAULT 'manual',  -- default | manual | 404
	enabled     INTEGER NOT NULL DEFAULT 1,
	hits        INTEGER NOT NULL DEFAULT 0,
	last_hit_at INTEGER,
	created_by  INTEGER,
	created_at  INTEGER NOT NULL
);

INSERT OR IGNORE INTO blocked_paths (pattern, note, source, created_at) VALUES
	('*.php', 'PHP: this site runs none', 'default', 0),
	('*.php5', 'PHP', 'default', 0),
	('*.phtml', 'PHP', 'default', 0),
	('*.asp', 'ASP', 'default', 0),
	('*.aspx', 'ASP.NET', 'default', 0),
	('*.jsp', 'Java pages', 'default', 0),
	('*.cgi', 'CGI scripts', 'default', 0),
	('/cgi-bin*', 'CGI scripts', 'default', 0),
	('/wp-admin*', 'WordPress', 'default', 0),
	('/wp-content*', 'WordPress', 'default', 0),
	('/wp-includes*', 'WordPress', 'default', 0),
	('/wp-json*', 'WordPress', 'default', 0),
	('/wordpress*', 'WordPress', 'default', 0),
	('/xmlrpc*', 'WordPress XML-RPC', 'default', 0),
	('/.env*', 'Environment files with secrets', 'default', 0),
	('*/.env', 'Environment files with secrets', 'default', 0),
	('/.git*', 'Git repository', 'default', 0),
	('/.svn*', 'Subversion repository', 'default', 0),
	('/.hg*', 'Mercurial repository', 'default', 0),
	('/.aws*', 'Cloud credentials', 'default', 0),
	('/.ssh*', 'SSH keys', 'default', 0),
	('/.docker*', 'Docker config', 'default', 0),
	('/.vscode*', 'Editor config', 'default', 0),
	('/.idea*', 'Editor config', 'default', 0),
	('/.ds_store', 'macOS folder file', 'default', 0),
	('/phpmyadmin*', 'Database admin', 'default', 0),
	('/pma*', 'Database admin', 'default', 0),
	('/adminer*', 'Database admin', 'default', 0),
	('/vendor/*', 'PHP packages (PHPUnit exploit)', 'default', 0),
	('/actuator*', 'Spring Boot', 'default', 0),
	('/server-status*', 'Apache status', 'default', 0),
	('/boaform*', 'Router exploit', 'default', 0),
	('/hnap1*', 'Router exploit', 'default', 0),
	('*.sql', 'Database dumps', 'default', 0),
	('*.sql.gz', 'Database dumps', 'default', 0),
	('*.bak', 'Backup files', 'default', 0),
	('*.swp', 'Editor swap files', 'default', 0),
	('/backup*', 'Backups', 'default', 0),
	('/config.json', 'Config probing', 'default', 0),
	('/docker-compose*', 'Docker config', 'default', 0),
	('/web.config', 'IIS config', 'default', 0),
	('/owa/*', 'Outlook Web Access', 'default', 0),
	('/autodiscover/*', 'Exchange autodiscover', 'default', 0),
	('/ecp/*', 'Exchange control panel', 'default', 0);
