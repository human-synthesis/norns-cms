# norns-cms

**A content management system that is an ordinary Norns app.**

Admin at `/admin`, a public site at `/`, rich Markdown articles and block-built pages, staff and member accounts, a cookie consent manager that gates analytics, and SEO / GEO built in. A new content type, block or setting is one feature folder. Built on [`@human-synthesis/norns`](https://github.com/human-synthesis/norns), [`norns-core`](https://github.com/human-synthesis/norns-core) and [`norns-ui`](https://github.com/human-synthesis/norns-ui); started from [`norns-app`](https://github.com/human-synthesis/norns-app).

## Setup

```sh
bun create human-synthesis/norns-cms my-site
cd my-site
bun install
bun run db:migrate                                   # local D1 under .wrangler/state (from wrangler.toml)
CMS_ADMIN_EMAIL=you@example.com CMS_ADMIN_PASSWORD=change-me bun run db:seed
bun run dev                                          # http://localhost:5173
```

Sign in at `/login`, then create a page or an article. Pages resolve at `/<slug>`, articles at `/articles/<slug>` with archives per category, tag and author. Staff see drafts and scheduled entries as previews. Pick a home page under Settings → Site.

The app runs on Cloudflare D1 whenever a `DB` binding is present (production through wrangler, `bun run dev` through adapter-cloudflare's platform proxy) and on a local SQLite file otherwise (`bun test`, or a Bun/Node server without D1: `bun run migrate up` + `bun run cms:seed`, path from `DATABASE_URL`, default `data/app.db`). Both go through the same async surface in `src/lib/norns/auth/server/db.js`.

## Deploy

Live at <https://norns-cms.humansynthesis.ai> on Cloudflare Workers + D1 (see `wrangler.toml`; the D1 database `norns-cms` already exists and its id is committed there). For a fresh account, create it once with `bunx wrangler d1 create norns-cms` and paste the printed `database_id` into `wrangler.toml`.

To ship a change, either `bun run deploy` (build, apply migrations remotely, `wrangler deploy`) with a logged-in wrangler, or push to `main`: `.github/workflows/deploy.yml` runs lint, check, tests, build, `wrangler d1 migrations apply norns-cms --remote` and `wrangler deploy`. It needs the `CLOUDFLARE_API_TOKEN` (Workers Scripts: Edit, D1: Edit) and `CLOUDFLARE_ACCOUNT_ID` repository secrets. Create the first admin on the production database with `bun run db:seed:remote`.

Migrations live in `migrations/<feature>/`; `bun run d1:sync` mirrors them into the flat `migrations/d1/` folder wrangler reads. Run it after adding a migration and commit both.

## Features

| | |
|---|---|
| **Content** | code-defined types with tabs, 20 field types, page blocks (hero, text, columns, call to action, image, gallery, FAQ, video / embed, article list, location, cookie table), drafts → review → publish or schedule, members-only visibility, revisions with diff and restore, draft autosave |
| **Articles** | summary (TL;DR), cover, rich Markdown body, categories (nested) and tags, several authors with author pages, key facts, sources, featured, reading time, related articles, RSS per site and category |
| **Rich Markdown** | GFM tables and task lists, highlighted code, media-library images with captions, `:::note` / `:::tip` / `:::warning` callouts, `::youtube{id=…}` / `::vimeo{id=…}` / `::embed{url=…}` consent-aware embeds, `::article{slug=…}` cards, `[[toc]]`; sanitised; HTML cached per entry. Editor with toolbar, shortcuts, server preview, paste / drop image upload |
| **Media** | library on R2, type sniffing (no SVG/HTML), 10 MB limit, alt text and captions, usage tracking, optional Cloudflare Image Resizing srcsets |
| **Users** | staff and member accounts, built-in roles (owner, admin, editor, author, viewer, member) plus custom roles from declared capabilities, invites, email verification, password reset, open / invite-only / closed registration, two-factor sign-in (TOTP with a QR code for setup + recovery codes; required or optional separately for CMS staff and for members), sessions you can revoke, suspension, rate-limited sign-in, audit log, member self-service (profile, data export, account deletion) |
| **Form protection** | every public form registered and tracked: managed honeypot and signed render time, Cloudflare Turnstile with the keys set in the admin, per-form switches, 30-day counts of passed and blocked submissions |
| **Consent** | cookie banner with equal Accept / Reject / Customise, per-category choices, regional rules (EU/EEA, UK, CH, rest: opt-in / opt-out / notice) from Cloudflare's country, Global Privacy Control, script presets (GA4, GTM, Plausible, Matomo, Clarity, Hotjar, Meta Pixel, LinkedIn) and custom code, Google Consent Mode v2, scripts inert until consent and activated in place, withdrawal clears cookies, consent log without IPs + CSV export, policy versions, cookie declaration |
| **SEO & GEO** | per-entry meta, canonical, robots, social image, schema type; title template; Open Graph / X cards; JSON-LD (Organization or LocalBusiness, WebSite, BlogPosting with authors, citations and abstract, BreadcrumbList, FAQPage); sitemaps with images; robots.txt with per-bot AI crawler switches; `llms.txt`, `llms-full.txt` and a Markdown twin of every page (`/<path>.md`); automatic 301s on URL changes, redirect manager with CSV import, 404 log; live editor checks and a site report; local business address, hours and coordinates |
| **Localisation** | localisations (markets) on sub-folders (`/en/…`, `/ch/de/…`), sub-domains or their own domains, each with several languages; content per language linked as translations, with "source changed" tracking and optional AI translation (Claude); translated URL words, categories and tags; interface texts built in for en, de, fr, es, it, ro and editable per language; translatable settings; language and region switchers; hreflang, per-locale sitemaps, feeds and llms.txt; member emails in the member's language |
| **Forms** | contact and newsletter forms built in, any number of custom forms with a field builder (text, email, phone, choices, consent …), translated labels, spam protection without captchas, an inbox with new / read / archived / spam and CSV export, email notifications, a Form page block and an optional footer form |
| **API** | management API at `/api/v1` for everything in the admin, with API keys limited by capability, OpenAPI 3.1 at `/api/v1/openapi.json`, docs in the admin and in [`docs/API.md`](docs/API.md) |
| **Platform** | Cloudflare Workers + D1 + R2, nonce-based CSP with `strict-dynamic`, security headers, daily maintenance cron, mail through Aiosis Emails (or a log in dev), read API as JSON or TRON |

## Add a content type

```
src/lib/norns/events/
  shared/types.c       export eventType := { id: 'event', label: 'Event', labelPlural: 'Events', basePath: '/events', listable: true, fields: […] }
  server/module.c      export default (app) => registerType app, eventType
```

That is the whole change: the admin sidebar, the list, the editor (with its SEO tab), `/events`, `/events/<slug>`, the sitemap, `llms.txt` and `/api/content/event` all appear. Field types are listed in `src/lib/norns/content/server/fields.c`; add one there (schema factory) and in `content/ui/FieldEditor.n` (control).

## Commands

```sh
bun run dev            # dev server
bun run build          # production build (.svelte-kit/cloudflare)
bun run db:migrate     # apply migrations to the local D1 (db:migrate:remote for production)
bun run db:seed        # first admin on the local D1 (db:seed:remote for production)
bun run d1:sync        # mirror migrations/<feature>/ into migrations/d1/ for wrangler
bun run migrate up     # SQLite-file mode: apply migrations (DATABASE_URL=file:... to override the path)
bun run cms:seed       # SQLite-file mode: create the first admin
bun run preview        # wrangler dev over the production build
bun run deploy         # build + remote migrations + wrangler deploy (needs wrangler login)
bun run lint           # norns lint
bun run check          # norns check
bun test               # feature tests through the DI container + architecture rules
```

## Layout

```
src/hooks.server.c          security → session → SEO → consent handles
src/routes/(site)/          public site and the members' account area
src/routes/admin/           the admin
src/lib/norns/<feature>/    auth · settings · content · pages · articles · media · mail · seo · consent
worker/index.js             Worker entry with the daily maintenance cron
migrations/                 per-feature SQL, mirrored into migrations/d1/ for wrangler
```

`CLAUDE.md` describes each feature, the extension points and the conventions.

Production secrets (`wrangler secret put`): `CMS_SECRET` (2FA encryption), `CRON_SECRET` (maintenance cron), `AIOSIS_API_KEY` (mail through Aiosis Emails). The R2 bucket `norns-cms-media` must exist before deploying (`bunx wrangler r2 bucket create norns-cms-media`).

## License

MIT © Daniel Teodoroiu / [Human Synthesis](https://humansynthesis.ai). Built on top of [SvelteKit](https://github.com/sveltejs/kit) and [Svelte](https://github.com/sveltejs/svelte) © Svelte Contributors, MIT licensed.
