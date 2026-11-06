// adapter-cloudflare writes the Worker entry (wrangler.toml `main`,
// worker/index.js) on every build, with a fetch handler only. This adds
// scheduled(): each cron trigger becomes POST /api/cron through the same
// fetch handler, with CRON_SECRET and the trigger's expression (x-cron).
// Runs after `norns build` (package.json "build").
import { readFileSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';

const file = join(import.meta.dir, '..', 'worker', 'index.js');
const MARK = '// --- tools/worker-cron.js';
let src = readFileSync(file, 'utf8');
if (src.includes(MARK)) src = src.slice(0, src.indexOf(MARK));
// adapter-cloudflare 8 (SvelteKit 3) ends the entry with `export default {`.
if (/^export default \{/m.test(src)) src = src.replace(/^export default \{/m, 'const worker_default = {');
else if (!/^const worker_default = \{/m.test(src)) {
	console.error('worker-cron: `export default {` not found in worker/index.js; the adapter output changed');
	process.exit(1);
}
src += `${MARK}: cron triggers → POST /api/cron ---
worker_default.scheduled = (controller, env2, ctx) => {
  const base = String(env2.CMS_PUBLIC_URL || "http://localhost").replace(/\\/+$/, "");
  const req = new Request(\`\${base}/api/cron\`, {
    method: "POST",
    headers: { "x-cron-key": env2.CRON_SECRET || "", "x-cron": controller.cron || "" }
  });
  ctx.waitUntil(
    worker_default.fetch(req, env2, ctx).then(async (res) => {
      if (!res.ok) console.error("cron", controller.cron, res.status, (await res.text()).slice(0, 500));
    })
  );
};
export default worker_default;
`;
writeFileSync(file, src);
console.log('worker-cron: scheduled() added to worker/index.js');
