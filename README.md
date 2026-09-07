# norns-cms

**A content management system that is an ordinary Norns app.**

Admin at `/admin`, a public site at `/`, pages and posts written in Markdown, and a content engine where a new content type is one feature folder. Built on [`@human-synthesis/norns`](https://github.com/human-synthesis/norns), [`norns-core`](https://github.com/human-synthesis/norns-core) and [`norns-ui`](https://github.com/human-synthesis/norns-ui); started from [`norns-app`](https://github.com/human-synthesis/norns-app).

## Setup

```sh
bun create human-synthesis/norns-cms my-site
cd my-site
bun install
bun run db:migrate                                   # local D1 under .wrangler/state (from wrangler.toml)
CMS_ADMIN_EMAIL=you@example.com CMS_ADMIN_PASSWORD=change-me bun run db:seed
bun run dev                                          # http://localhost:5173
```

Sign in at `/login`, then create a page or a post. Published pages resolve at `/<slug>`, posts at `/posts/<slug>`, and `/posts` lists them. Signed-in users see drafts as previews.

The app runs on Cloudflare D1 whenever a `DB` binding is present (production through wrangler, `bun run dev` through adapter-cloudflare's platform proxy) and on a local SQLite file otherwise (`bun test`, or a Bun/Node server without D1: `bun run migrate up` + `bun run cms:seed`, path from `DATABASE_URL`, default `data/app.db`). Both go through the same async surface in `src/lib/norns/auth/server/db.js`.

## Deploy

Live at <https://norns-cms.humansynthesis.ai> on Cloudflare Workers + D1 (see `wrangler.toml`; the D1 database `norns-cms` already exists and its id is committed there). For a fresh account, create it once with `bunx wrangler d1 create norns-cms` and paste the printed `database_id` into `wrangler.toml`.

To ship a change, either `bun run deploy` (build, apply migrations remotely, `wrangler deploy`) with a logged-in wrangler, or push to `main`: `.github/workflows/deploy.yml` runs lint, check, tests, build, `wrangler d1 migrations apply norns-cms --remote` and `wrangler deploy`. It needs the `CLOUDFLARE_API_TOKEN` (Workers Scripts: Edit, D1: Edit) and `CLOUDFLARE_ACCOUNT_ID` repository secrets. Create the first admin on the production database with `bun run db:seed:remote`.

Migrations live in `migrations/<feature>/`; `bun run d1:sync` mirrors them into the flat `migrations/d1/` folder wrangler reads. Run it after adding a migration and commit both.

## What's in the MVP

| | |
|---|---|
| **Admin** | dashboard with counts and recent edits, a collection list per content type (search, status filter), a generic editor (save, publish / unpublish, delete), draft and revision tracking, dark mode |
| **Public site** | header built from published pages flagged *Show in navigation* plus listable types, latest posts on the home page, Markdown rendering, draft preview for signed-in users |
| **Content engine** | content types declared in code, compiled to valibot schemas; field types `text`, `textarea`, `markdown`, `number`, `boolean`, `datetime`, `select`, `tags`; one `entries` table with JSON field values plus `revisions` |
| **Auth** | scrypt passwords, cookie sessions, `admin` / `editor` roles |
| **API** | `GET /api/content/<type>` and `/api/content/<type>/<slug>`, JSON or TRON by `Accept` |
| **Storage** | Cloudflare D1 in production and dev, SQLite (`bun:sqlite`, or `better-sqlite3` under Node) for tests and D1-less servers; hand-written SQL behind one async surface |

Not yet: media uploads, menus and settings pages, users UI, block-based pages, a rich text editor, scheduled publishing. See `docs/plans/PLAN_CMS_V1.md` in the workspace for the roadmap.

## Add a content type

```
src/lib/norns/events/
  shared/types.c       export eventType := { id: 'event', label: 'Event', labelPlural: 'Events', basePath: '/events', listable: true, fields: […] }
  server/module.c      export default (app) => registerType app, eventType
```

That is the whole change: the admin sidebar, the list, the editor, `/events`, `/events/<slug>` and `/api/content/event` all appear. Field types are listed in `src/lib/norns/content/server/fields.c`; add one there (schema factory) and in `content/ui/FieldEditor.n` (control).

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
src/
  hooks.server.c                 boots the runtime, adds the session handle
  routes/(site)/                 public frontend
  routes/login/                  sign in
  routes/admin/                  admin shell, dashboard, content/[type], content/[type]/[id]
  routes/api/content/            read API
  lib/norns/auth/                db owner (db.js: SQLite + D1 adapters), users, sessions
  lib/norns/content/             registry, field types, entries, revisions, rendering, routing
  lib/norns/pages/, posts/       one content type each
migrations/<feature>/            SQL, forward-only (source of truth)
migrations/d1/                   flat mirror for wrangler, generated by tools/d1-sync.js
tests/                           bun tests
tools/seed.js                    first admin user (SQLite file or D1)
wrangler.toml                    Workers config, D1 binding, custom domain
.github/workflows/deploy.yml     lint, check, test, build, migrate, deploy on push to main
```

## License

MIT © Daniel Teodoroiu / [Human Synthesis](https://humansynthesis.ai). Built on top of [SvelteKit](https://github.com/sveltejs/kit) and [Svelte](https://github.com/sveltejs/svelte) © Svelte Contributors, MIT licensed.
