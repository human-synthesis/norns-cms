// Browser smoke test (Playwright + Chromium) against a running server:
//
//   bun run build && bunx wrangler dev --port 8787   (or bun run dev)
//   E2E_URL=http://localhost:8787 E2E_EMAIL=… E2E_PASSWORD=… bun tests/e2e/browser.mjs
//
// Checks what only a browser can: nothing third-party loads before consent,
// "Accept all" activates scripts and embeds in place, withdrawing through
// "Cookie settings" reloads without them, and the admin editor (tabs, live
// SEO preview, Markdown preview, blocks editor) works without console errors.
// Third-party hosts are stubbed, so no request leaves the machine.
import { chromium } from 'playwright';

const BASE = process.env.E2E_URL ?? 'http://localhost:8787';
const EMAIL = process.env.E2E_EMAIL;
const PASSWORD = process.env.E2E_PASSWORD;
const ARTICLE = process.env.E2E_ARTICLE ?? '/articles/hello-norns';

let failures = 0;
const check = (ok, label) => {
	console.log(`${ok ? 'ok  ' : 'FAIL'} ${label}`);
	if (!ok) failures++;
};

const browser = await chromium.launch();
const ctx = await browser.newContext();
const thirdParty = [];
const errors = [];
await ctx.route(/googletagmanager\.com|google-analytics\.com|youtube-nocookie\.com|youtube\.com|vimeo\.com/, (route) => {
	thirdParty.push(route.request().url());
	const type = route.request().resourceType();
	route.fulfill({ status: 200, contentType: type === 'script' ? 'text/javascript' : 'text/html', body: type === 'script' ? 'window.__stubLoaded=(window.__stubLoaded||0)+1' : '<p>stub</p>' });
});
const page = await ctx.newPage();
page.on('console', (m) => m.type() === 'error' && !/Failed to load resource/.test(m.text()) && errors.push(m.text()));
page.on('pageerror', (e) => errors.push(String(e)));

// --- consent ------------------------------------------------------------------
await page.goto(BASE + ARTICLE);
await page.waitForLoadState('networkidle');
check(await page.locator('.cookie-banner').isVisible(), 'banner shows on first visit');
check(thirdParty.length === 0, `nothing third-party before consent (${thirdParty.length})`);

await page.getByRole('button', { name: 'Accept all' }).first().click();
await page.waitForTimeout(800);
check(!(await page.locator('.cookie-banner').isVisible()), 'banner closes after Accept all');
check(thirdParty.some((u) => u.includes('googletagmanager')), 'analytics script activated in place');
check((await page.evaluate(() => window.__cmsConsent?.analytics)) === true, 'page state updated');

await page.goto(BASE + '/');
await page.waitForLoadState('networkidle');
check(!(await page.locator('.cookie-banner').isVisible()), 'choice remembered on the next page');
const iframes = await page.locator('.embed iframe').count();
check(iframes > 0 || (await page.locator('.embed').count()) === 0, `embeds load with marketing consent (${iframes})`);

thirdParty.length = 0;
await page.locator('.site-footer').getByRole('button', { name: 'Cookie settings' }).click();
const dialog = page.locator('dialog');
await dialog.waitFor();
const switches = dialog.locator('input[type="checkbox"]');
for (let i = 0; i < (await switches.count()); i++) if (await switches.nth(i).isChecked()) await switches.nth(i).uncheck({ force: true });
await Promise.all([page.waitForEvent('load'), dialog.getByRole('button', { name: 'Save choices' }).click()]);
await page.waitForLoadState('networkidle');
check(thirdParty.length === 0, `after withdrawing, nothing third-party loads (${thirdParty.length})`);
check((await page.evaluate(() => window.__cmsConsent)).analytics === false, 'withdrawal applied');

// --- admin editor -------------------------------------------------------------
if (EMAIL && PASSWORD) {
	await page.goto(BASE + '/login');
	await page.fill('input[name="email"]', EMAIL);
	await page.fill('input[name="password"]', PASSWORD);
	await Promise.all([page.waitForURL(/\/admin/), page.click('button[type="submit"]')]);
	check(page.url().includes('/admin'), 'staff sign-in');

	await page.goto(BASE + '/admin/content/article/1');
	await page.waitForLoadState('networkidle');
	await page.getByRole('tab', { name: 'SEO' }).click();
	check(await page.locator('.seo-preview-title').isVisible(), 'SEO tab with live preview');
	await page.fill('input[name="seoTitle"]', 'A custom search title');
	check((await page.locator('.seo-preview-title').textContent()).includes('A custom search title'), 'SEO preview follows typing');
	await page.getByRole('tab', { name: 'Content' }).click();
	await page.locator('.md-input').first().fill('## Typed\n\nWith **bold**.');
	await page.getByRole('button', { name: 'Preview', exact: true }).first().click();
	await page.waitForSelector('.md-preview h2');
	check((await page.locator('.md-preview').first().innerHTML()).includes('<strong>bold</strong>'), 'Markdown preview renders on the server');

	page.once('dialog', (d) => d.accept());
	await page.goto(BASE + '/admin/content/page/2');
	await page.waitForLoadState('networkidle');
	const before = await page.locator('.block-card').count();
	await page.locator('.blocks-palette').getByRole('button', { name: 'Text', exact: true }).click();
	check((await page.locator('.block-card').count()) === before + 1, 'blocks editor adds a block');
	await page.locator('.block-card').last().locator('.md-input').fill('Added in the **browser**.');
	await Promise.all([page.waitForURL(/saved=/), page.getByRole('button', { name: 'Save', exact: true }).click()]);
	check((await page.locator('.block-card').count()) === before + 1, 'block persisted after save');
	await page.goto(BASE + '/');
	check((await page.content()).includes('Added in the <strong>browser</strong>.'), 'new block renders on the site');
}

check(errors.length === 0, `no console or page errors${errors.length ? ': ' + errors.slice(0, 3).join(' | ') : ''}`);
await browser.close();
console.log(failures ? `\n${failures} check(s) failed` : '\nall browser checks passed');
process.exit(failures ? 1 : 0);
