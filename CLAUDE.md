# norns-cms — agent guide

A content management system that is an ordinary Norns app: SvelteKit + Svelte 5 with Pug + Civet (`.n` components, `.c` modules), the `@human-synthesis/norns` runtime (feature folders, DI container, `page` / `route` wrappers with valibot validation) and `@human-synthesis/norns-ui`. Started from `norns-app`.

## Layout

```
src/
  hooks.server.c        boots the runtime; handles in order: securityHandle (nonce + CSP), sessionHandle
                        (guards /admin for staff and /account for members, every method), seoHandle
                        (redirects, /<path>.md twins, 404 log), consentHandle (consent state, script injection)
  routes/
    (site)/             public site: home (a chosen page or latest articles), [...slug] (any entry),
                        articles/ (+ category/tag archives), authors/[slug], account/ (members area)
    admin/              staff area; content/[type] (generic list) and [id] (generic editor, revisions),
                        media, taxonomy/[taxonomy], users, roles, audit, profile, settings/[group], seo,
                        consent (stats, scripts, log), mail, maintenance; api/ (preview, media JSON)
    login/ reset/ invite/ verify/    sign-in, two-factor step, one-time links
    api/v1/[...path]   management API (bearer API keys); api/consent, api/content (public read), api/cron
    sitemap.xml  sitemap-[type].xml  robots.txt  llms.txt  llms-full.txt  feed.xml  media/[...key]
  lib/norns/<feature>/  server/module.c (DI + registrations), repo.c / service.c (private),
                        server/public.c (the ONLY cross-feature import), shared/ (pure), ui/ (.n)
  lib/components/       auto-imported app components (Stat, EmptyState, PageHead)
worker/index.js         Worker entry: SvelteKit's worker + scheduled() → POST /api/cron
migrations/<feature>/   source of truth (timestamped; applied in file-name order across features)
migrations/d1/          flat mirror for wrangler — run `bun run d1:sync` after adding a migration
tests/                  bun tests through the DI container; tests/e2e/browser.mjs (Playwright, opt-in)
```

Features:

| Feature | Owns |
|---|---|
| `auth` | the `db` token; staff and client users, roles (rows) with capabilities (declared in code), sessions, invites / verification / reset / 2FA tokens, rate limits, audit log, `sessionHandle` |
| `settings` | settings groups (`registerSettings`), env access (`envVar`, `binding`, `siteOrigin`), `securityHandle`, `runMaintenance` |
| `content` | the engine: type registry, field types, field groups, blocks, hooks, option providers, admin nav; entries, revisions, rich Markdown renderer with cached HTML, public routing, members-only locking |
| `pages` | the page type (Markdown or blocks layout) and the general blocks (hero, text, columns, CTA, image, gallery, FAQ, embed) |
| `articles` | the article type, categories and tags (`terms`), authors, review notifications, article-list block |
| `media` | uploads (sniffed type, size limit) to R2 (`MEDIA`) or `data/media`, usage tracking, `/media/*` |
| `mail` | Resend over HTTP when `RESEND_API_KEY` is set; otherwise logged to `mail_log` (dev "inbox") |
| `seo` | SEO field group and settings, head tags, JSON-LD, sitemaps, robots (AI crawler toggles), llms.txt, Markdown twins, feeds, redirects, 404 log, editor checks, location block |
| `consent` | banner settings, regional rules, GPC, script presets, consent records, injection + loader, cookie table block |
| `api` | API keys (hashed, capability-limited, acting for their creator), the endpoint registry, router (`/api/v1`), OpenAPI document, admin key management and docs |

**Extension points** (all called from a feature's `module.c`, all bindings on the root container so order never matters): `registerType`, `registerFieldGroup`, `registerBlock` (a block's `resolve(props, { c, viewer, entry })` runs per request; add its component to `pages/ui/BlockRenderer.n`), `registerHook` (`beforeSave` may return replacement data; `afterSave`, `status`, `slug`, `delete`), `registerOptions`, `registerAdminNav` (from `content/server/public`); `registerCapabilities` (from `auth/server/public`); `registerSettings` (from `settings/server/public`).

**Adding a content type** is a feature folder with `shared/types.c` (`id`, `label`, `labelPlural`, `icon`, `basePath`, `listable`, `tabs?`, `fields`) and a `server/module.c` that calls `registerType`. The admin list, editor (tabs, SEO tab, revisions), public routing, sitemap, llms.txt and API pick it up. Field types: `text`, `textarea`, `markdown`, `code`, `number`, `boolean`, `datetime`, `select`, `tags`, `lines`, `email`, `url`, `image`, `reference`, `references`, `users`, `term`, `blocks`, `repeater`, `json` (see `content/server/fields.c`, `content/ui/FieldEditor.n`, `content/ui/JsonField.n`). `showIf: { field, equals }` hides a field in the editor; `default` applies when a key is absent.

**Permissions.** Routes check capabilities with `requireCap(user, 'content.publish')`; the admin nav hides what the user lacks. Owners and admins hold `*`; editor, author, viewer and member are built-in rows in `roles`; custom roles pick from the capabilities features declare. Only owners manage owners, the last active owner cannot be removed, nobody changes their own role or status.

**Secrets** (`wrangler secret put`, `.dev.vars` locally — never in `wrangler.toml`): `CMS_SECRET` (encrypts 2FA secrets), `CRON_SECRET` (daily maintenance), `RESEND_API_KEY` (mail). Vars: `CMS_PUBLIC_URL`, `MAIL_FROM`.

Import project code explicitly (`import { cms } from '$lib/norns/content/server/public'`); inside `src/lib/norns/**` use relative paths (`../../content/server/public`) so `bun test` can resolve them. Only framework helpers and norns-ui components (plus `src/lib/components`) are auto-imported.

## Every feature ships its API

The CMS has a management API at `/api/v1` (API keys from Admin → API; guide in `docs/API.md`; live reference at Admin → API → Documentation and `/api/v1/openapi.json`). **Anything a person can do in the admin must also be possible through the API.** When you add or change a feature:

1. Register its endpoints in `<feature>/server/api.c` with `registerEndpoint(app, { method, path, capability, tag, summary, query?, body?, handler })` and call that from the feature's `module.c`. Shape and helpers (`apiError`, `validationError`, `paging`, `need`, `allow`): `src/lib/norns/api/server/router.js` and `public.c`.
2. Reuse the feature's facade and the same permission rules as the admin route (`canEditEntry`, owner rules, capability per action). Never add an API-only shortcut around a check.
3. Audit writes with `{ via: 'api' }` in the meta, like the admin does.
4. Give every endpoint a `summary`, a `tag`, its `capability` (or `null` for any key), `query` docs and a `body` example: the reference and OpenAPI are generated from them, and `tests/api.test.js` fails on undocumented endpoints.
5. Add a test in `tests/api.test.js` driving the endpoint through `handleApi`, and update `docs/API.md` when you add a new concept (not for each endpoint — the reference covers those).

API keys themselves are managed only in the admin, never through the API.

## Commands

```sh
bun run migrate up     # apply migrations to data/app.db (DATABASE_URL=file:... to override)
bun run cms:seed       # first admin: CMS_ADMIN_EMAIL / CMS_ADMIN_PASSWORD from env, or argv
bun run dev            # http://localhost:5173 — admin at /admin, sign in at /login
bun run lint           # norns lint — Civet / Pug pitfall scan (templates + script blocks)
bun run check          # norns check — preprocess + compile every .n / .c; file:line:column errors
bun run build          # full Vite build
bun test               # tests/*.test.js (features through the DI container + architecture rules)
bun tests/e2e/browser.mjs         # Playwright smoke test against a running server (E2E_URL, E2E_EMAIL, E2E_PASSWORD)
bunx norns diag <file>            # JS that Civet emits for a .c / .n script
bunx norns diag --template <f.n>  # Svelte source the compiler sees after Pug / Civet / auto-import
```

```sh
bun run db:migrate     # local D1 (.wrangler/state), read by `bun run dev` via the adapter's platform proxy
bun run db:seed        # first admin on the local D1; db:migrate:remote / db:seed:remote for production
bun run preview        # wrangler dev (workerd) over the production build — the closest thing to production
bun run deploy         # build + remote migrations + wrangler deploy; needs a logged-in wrangler
```

**Storage.** `auth/server/db.js` exposes one async surface (`all`, `get`, `run`, `exec`) over two backends: D1 when the request carries `event.platform.env.DB` (production, and `bun run dev` through adapter-cloudflare's platform proxy), otherwise the SQLite file at `DATABASE_URL` (`bun test`, a Bun/Node server without D1; under Node install `better-sqlite3`). Because `db` is resolved per request, `db`, repos and services are **transient** bindings (`app.bind`), never `single`. Every repo/service/facade call that touches the database returns a promise: always `await`.

Deploys and pushes are user-gated: never push or run `wrangler deploy` without an explicit go-ahead in the current conversation.

## Working with norns (Civet + Pug + Svelte 5)

Civet is great for app code but has rough edges on advanced syntax. **When something doesn't parse or compile in Civet, drop to plain `.js` for that file — don't fight the parser.** Libraries and dense generator/stream/type code belong in `.js` (see `auth/server/sqlite.js`, `password.js`); routes, components, and feature code stay in `.c`/`.n`.

### Civet pitfalls — do not write these

| Don't | Do | Why |
|---|---|---|
| `if x isnt y` | `if x !== y` | `isnt` compiles to a bare identifier reference at runtime |
| `async *foo()` as class method | callback `foo(onEvent)` or top-level `async function*` | parser rejects async generators in class shorthand |
| `value := $state ''` then later `value = 'x'` | `value .= $state ''` | `:=` creates `const`; reassigning `$state` needs `let` (which `.=` produces) |
| `raw: unknown` / `raw: any` as a let-with-type | `raw .= null` (no annotation) or `let raw: any = null` | bare `Type` annotations get read as identifier references |
| `loop := …`, or any variable named after a Civet keyword | another name (`onward`, `cycle`) | `loop` starts Civet's infinite-loop statement; the parse fails on the next token |
| `if (a)[k]` / `if (a).b()` / `x = y if (a)[k]` | name it first: `m := a`, then `if m[k]` | a parenthesised `if` condition ends at its `)`, so `[k]` / `.b` become the body (and a postfix `if (…)` turns the left side into a call) |
| a class method or facade key named `type` | `getType` | `type` starts a TypeScript alias declaration in Civet |
| a script-level function or variable named `state` (or `derived`, `effect`) in a `.n` file | another name (`keyState`) | collides with the compiled runes: SSR fails with `Cannot access 'state2' before initialization` |

### Pug / `.n` pitfalls — do not write these

| Don't | Do | Why |
|---|---|---|
| `{@html foo}` as a top-level Pug line | `\| {@html foo}` (pipe-prefix) | Pug parses leading `{` as a malformed tag |
| `#{expr}` interpolation in template | `{expr}` (Svelte interpolation) | `#{...}` evaluates at preprocess time, with no runtime data in scope; SSR 500s |
| `attr="#{expr}"` attribute interpolation | `attr!="{expr}"` (Svelte) | same reason |
| `+each('row of rows')` | `+each('rows as row')` (Svelte `as` form, optionally `(row.id)` key) | the `of` form is copied verbatim into the block and the Svelte compiler rejects it |
| `#{expr}` or `.a.b` shorthand across a line break | keep one element per line | Pug is line-oriented; parse errors surface on the *next* line |

Template syntax that works: `+if('cond')` / `+elseif('cond')` / `+else`, `+each('items as item (item.id)')`, `+snippet('name', arg)` with `| {@render name(x)}`, `attr!="{expr}"` for Svelte expressions, `.a.b` class shorthand (Tailwind variants and fractions like `.hover:bg-x.gap-2.5` are rewritten for you).

### SvelteKit / norns gotchas

- **Workers forbid randomness and I/O in global scope.** `randomBytes()`, `crypto.getRandomValues`, `fetch` or timers at module top level crash every request under workerd (`Disallowed operation called within global scope`) while working under Bun and Vite. Compute such values lazily inside a handler or function, and smoke-test the production build with `wrangler dev`.
- **norns-ui `Btn` and `Form` do not forward unknown attributes.** `formaction`, `oninput`, `onchange`, `data-*` on them are silently dropped. Use a plain `button.btn.btn-primary(type="submit" formaction="?/publish")`, and put input listeners on a wrapper element inside the form (events bubble).
- **norns-ui `Tabs` renders only the active panel.** Fields on other tabs are not in the DOM, so a form spanning tabs loses them on submit. For tabbed forms, toggle `hidden` on panels you render yourself.
- **`event.locals.container`** is the per-request DI scope set by `contextHandle` in `boot()`. Don't destructure DI directly from event args.
- **`event.locals.user`** is set by `sessionHandle` (auth) and reaches `page.load` / `page.actions` / `route()` handlers as `user`.
- **A layout `load` guard does not protect form actions.** SvelteKit runs `?/action` POSTs before any layout load, so `/admin` is enforced in `sessionHandle({ protect: ['/admin'] })` in `hooks.server.c`; the layout guard is only the friendly redirect. Any new protected area must be added to `protect`.
- **`page.actions` handlers receive `{ input, container, event, user }`**, not `params`. Read `event.params`.
- **Body parsing is content-type-driven.** `+page.server.c` actions get `formData()`; client `fetch` posting JSON must hit a `+server.c` endpoint, not a form action.
- **The editor validates inside the service**, not via `page.actions`' `input`, because the schema depends on `params.type`. Return `fail(400, { errors: issues, values })` so `<Form>` / `<Field>` show messages.
- **`svelte-check` never reads `.n` or `.c` files.** `bun run check:svelte` only covers `.js` / `.ts` / `.svelte`. It is not the pass signal for Norns code — `norns check` is.
- **Type-check is not feature verification.** Verify through the request path: `curl` against `bun run dev`, hitting POST actions and not just GET pages.

### Verification workflow — run before claiming done

1. **`bun run lint`** — must show 0 errors. `--json` for machine-readable output.
2. **`bun run check`** — must exit 0. `--json` available.
3. **`bun test`** — when you touched a feature folder or a boundary.
4. **`bun run build`** — the final word on whether everything compiles.
5. **`curl` through the dev server** — for any request-path change. Hit the real route, including POST/form actions. Form actions need `-H 'Origin: http://localhost:5173'` (SvelteKit's CSRF check) and `-H 'Accept: text/html'`; without the Accept header SvelteKit answers in its JSON action protocol (`{"type":"redirect",...}` with status 200), which is correct but not what a browser sees. Use `--data-urlencode 'body@file'` for multi-line Markdown; the container shell is `sh`, so `$'...'` quoting does not work.

### Things AI tends to get wrong here

- **Don't introduce ORMs.** Hand-written SQL in `repo.c`, through the `db` token.
- **Don't add `bcrypt`/`argon2`.** Passwords via `node:crypto.scrypt` (`auth/server/password.js`).
- **Never inject third-party code outside the consent manager.** Analytics, pixels and embeds go through consent scripts (or the embed directive/block) so they wait for consent.
- **Don't open the database anywhere but `auth/server/module.c`.** Everyone else calls `c.resolve('db')`; `tests/architecture.test.js` enforces it.
- **Don't use `single` for anything that resolves `db`.** On Workers the D1 binding is per request; a root singleton would capture the first request's binding.
- **After adding a migration, run `bun run d1:sync`** and commit `migrations/d1/` too, or production will not get it.
- **In `vite.config.js`, keep `server.allowedHosts: true`** when behind any reverse proxy in dev.
- **Container env vars are read at create time.** `podman-compose restart` does NOT pick up `.env` changes — use `up -d --force-recreate <service>`.
