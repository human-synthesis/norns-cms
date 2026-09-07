# norns-cms — agent guide

A content management system that is an ordinary Norns app: SvelteKit + Svelte 5 with Pug + Civet (`.n` components, `.c` modules), the `@human-synthesis/norns` runtime (feature folders, DI container, `page` / `route` wrappers with valibot validation) and `@human-synthesis/norns-ui`. Started from `norns-app`.

## Layout

```
src/
  hooks.server.c                     boots the runtime; eager-loads src/lib/norns/*/server/module.c; adds sessionHandle
  routes/
    (site)/                          public frontend: +layout (header from cms.nav), +page (latest posts), [...slug]
    login/                           sign-in form action
    admin/                           +layout.server.c guards every admin route; +layout.n is the shell
    admin/content/[type]/            generic collection list for ANY registered content type
    admin/content/[type]/[id]/       generic editor (id = 'new' creates); actions save / publish / remove
    api/content/[type]/[slug]?       read API, JSON or TRON
  lib/norns/<feature>/
    server/module.c                  DI registrations — the only file boot() touches
    server/repo.c, service.c         SQL and business logic (private to the feature)
    server/public.c                  the ONLY file routes / other features import
    shared/schema.c, types.c         valibot schemas, content type definitions
    ui/*.n                           feature-local components (imported explicitly)
migrations/<feature>/*.sql           source of truth; `bun run migrate up` (SQLite file) reads these
migrations/d1/*.sql                  flat mirror for wrangler (`bun run d1:sync`); `bun run db:migrate[:remote]`
tests/                               bun tests; tests/civet-loader.js lets bun import .c files
tools/seed.js                        first admin user (SQLite file, or --d1 [--remote])
wrangler.toml                        Workers config: D1 `DB` binding, custom domain, [vars]
```

Features: `auth` (owns `db`, users, sessions, `sessionHandle`), `content` (type registry, field types, entries, revisions, markdown rendering, routing), `pages` and `posts` (each registers one content type).

**Adding a content type** is a feature folder with `shared/types.c` (a plain object: `id`, `label`, `labelPlural`, `icon`, `basePath`, `listable`, `fields`) and a `server/module.c` that calls `registerType(app, myType)` from `content/server/public`. The admin list, editor, public routing and API pick it up with no route changes. Field types: `text`, `textarea`, `markdown`, `number`, `boolean`, `datetime`, `select`, `tags` (see `content/server/fields.c` and `content/ui/FieldEditor.n`).

Import project code explicitly (`import { cms } from '$lib/norns/content/server/public'`); inside `src/lib/norns/**` use relative paths (`../../content/server/public`) so `bun test` can resolve them. Only framework helpers and norns-ui components are auto-imported.

## Commands

```sh
bun run migrate up     # apply migrations to data/app.db (DATABASE_URL=file:... to override)
bun run cms:seed       # first admin: CMS_ADMIN_EMAIL / CMS_ADMIN_PASSWORD from env, or argv
bun run dev            # http://localhost:5173 — admin at /admin, sign in at /login
bun run lint           # norns lint — Civet / Pug pitfall scan (templates + script blocks)
bun run check          # norns check — preprocess + compile every .n / .c; file:line:column errors
bun run build          # full Vite build
bun test               # tests/*.test.js (features through the DI container + architecture rules)
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
| a class method or facade key named `type` | `getType` | `type` starts a TypeScript alias declaration in Civet |

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
- **Don't open the database anywhere but `auth/server/module.c`.** Everyone else calls `c.resolve('db')`; `tests/architecture.test.js` enforces it.
- **Don't use `single` for anything that resolves `db`.** On Workers the D1 binding is per request; a root singleton would capture the first request's binding.
- **After adding a migration, run `bun run d1:sync`** and commit `migrations/d1/` too, or production will not get it.
- **In `vite.config.js`, keep `server.allowedHosts: true`** when behind any reverse proxy in dev.
- **Container env vars are read at create time.** `podman-compose restart` does NOT pick up `.env` changes — use `up -d --force-recreate <service>`.
