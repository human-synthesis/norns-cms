// Hosts of sub-domain and domain localisations → Workers custom domains.
//
//   bun run domains                 list the hosts (local D1) and whether they answer
//   bun run domains -- --remote     same, from the production D1
//   bun run domains -- --remote --apply
//                                   attach every missing host as a custom domain of the
//                                   Worker (needs CLOUDFLARE_API_TOKEN with Workers Routes
//                                   and Zone read, and CLOUDFLARE_ACCOUNT_ID). The host's
//                                   zone must already be in the Cloudflare account.
const flags = new Set(process.argv.slice(2));
const remote = flags.has('--remote');
const DB = process.env.CMS_D1_NAME || 'norns-cms';
const WORKER = process.env.CMS_WORKER_NAME || 'norns-cms';

const q = Bun.spawnSync(['bunx', 'wrangler', 'd1', 'execute', DB, remote ? '--remote' : '--local', '--json', '--command', "SELECT code, name, mode, host, enabled FROM localisations WHERE mode != 'path'"], { stderr: 'pipe' });
if (q.exitCode !== 0) {
	console.error(q.stderr.toString());
	process.exit(1);
}
const rows = JSON.parse(q.stdout.toString())[0]?.results ?? [];
if (!rows.length) {
	console.log('No sub-domain or domain localisations. Sub-folder localisations need nothing.');
	process.exit(0);
}

const token = process.env.CLOUDFLARE_API_TOKEN;
const account = process.env.CLOUDFLARE_ACCOUNT_ID;
const cf = async (path, init = {}) => {
	const res = await fetch(`https://api.cloudflare.com/client/v4${path}`, { ...init, headers: { authorization: `Bearer ${token}`, 'content-type': 'application/json', ...(init.headers ?? {}) } });
	const body = await res.json().catch(() => ({}));
	if (!body.success) throw new Error(body.errors?.map((e) => e.message).join('; ') || `HTTP ${res.status}`);
	return body.result;
};

let attached = [];
if (token && account) {
	attached = (await cf(`/accounts/${account}/workers/domains`)).map((d) => d.hostname);
}

for (const r of rows) {
	const status = attached.length ? (attached.includes(r.host) ? 'attached' : 'not attached') : 'unknown (no Cloudflare token)';
	console.log(`${r.enabled ? '●' : '○'} ${r.host.padEnd(36)} ${r.name} (${r.mode}) — ${status}`);
}

if (!flags.has('--apply')) process.exit(0);
if (!token || !account) {
	console.error('\n--apply needs CLOUDFLARE_API_TOKEN and CLOUDFLARE_ACCOUNT_ID.');
	process.exit(1);
}
const zones = await cf(`/zones?per_page=500&account.id=${account}`);
for (const r of rows.filter((x) => x.enabled && !attached.includes(x.host))) {
	const zone = zones.filter((z) => r.host === z.name || r.host.endsWith('.' + z.name)).sort((a, b) => b.name.length - a.name.length)[0];
	if (!zone) {
		console.log(`✗ ${r.host}: its domain is not a zone in this Cloudflare account — add it there first.`);
		continue;
	}
	await cf(`/accounts/${account}/workers/domains`, { method: 'PUT', body: JSON.stringify({ environment: 'production', hostname: r.host, service: WORKER, zone_id: zone.id }) });
	console.log(`✓ ${r.host} attached to ${WORKER} (zone ${zone.name})`);
}
