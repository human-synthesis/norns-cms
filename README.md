# norns-cms

**A content management system that is an ordinary Norns app.**

Admin at `/admin`, a public site at `/`, pages and posts written in Markdown, and a content engine where a new content type is one feature folder. Built on [`@human-synthesis/norns`](https://github.com/human-synthesis/norns), [`norns-core`](https://github.com/human-synthesis/norns-core) and [`norns-ui`](https://github.com/human-synthesis/norns-ui); started from [`norns-app`](https://github.com/human-synthesis/norns-app).

## Setup

```sh
bun create human-synthesis/norns-cms my-site
cd my-site
bun install
bun run migrate up                                   # creates data/app.db
CMS_ADMIN_EMAIL=you@example.com CMS_ADMIN_PASSWORD=change-me bun run cms:seed
bun run dev                                          # http://localhost:5173
```

Sign in at `/login`, then create a page or a post. Published pages resolve at `/<slug>`, posts at `/posts/<slug>`, and `/posts` lists them. Signed-in users see drafts as previews.

## What's in the MVP

| | |
|---|---|
| **Admin** | dashboard with counts and recent edits, a collection list per content type (search, status filter), a generic editor (save, publish / unpublish, delete), draft and revision tracking, dark mode |
| **Public site** | header built from published pages flagged *Show in navigation* plus listable types, latest posts on the home page, Markdown rendering, draft preview for signed-in users |
| **Content engine** | content types declared in code, compiled to valibot schemas; field types `text`, `textarea`, `markdown`, `number`, `boolean`, `datetime`, `select`, `tags`; one `entries` table with JSON field values plus `revisions` |
| **Auth** | scrypt passwords, cookie sessions, `admin` / `editor` roles |
| **API** | `GET /api/content/<type>` and `/api/content/<type>/<slug>`, JSON or TRON by `Accept` |
| **Storage** | SQLite via `bun:sqlite` (or `better-sqlite3` under Node), hand-written SQL, `norns migrate` |

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
bun run build          # production build
bun run preview        # preview the build
bun run migrate up     # apply migrations (DATABASE_URL=file:... to override the path)
bun run cms:seed       # create the first admin
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
  lib/norns/auth/                db owner, users, sessions
  lib/norns/content/             registry, field types, entries, revisions, rendering, routing
  lib/norns/pages/, posts/       one content type each
migrations/<feature>/            SQL, forward-only
tests/                           bun tests
tools/seed.js                    first admin user
```

## License

MIT © Daniel Teodoroiu / [Human Synthesis](https://humansynthesis.ai). Built on top of [SvelteKit](https://github.com/sveltejs/kit) and [Svelte](https://github.com/sveltejs/svelte) © Svelte Contributors, MIT licensed.
