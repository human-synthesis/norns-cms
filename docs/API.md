# Management API

Everything you can do in the admin, you can do over HTTP: content, media, categories and tags, users and roles, settings, SEO, cookie consent, the audit log and maintenance. The full, always-current endpoint reference is in the admin under **API → Documentation**, and as an OpenAPI 3.1 document at `/api/v1/openapi.json`.

## Keys

Create a key under **Admin → API** (needs the `api.manage` capability). The key is shown once; only its SHA-256 is stored.

- A key **acts for the person who created it**. What it creates is authored by them, and the audit log records `you@example.com (API key "Name")`.
- A key holds **the capabilities you ticked**, and on every request only those its creator **still** holds. Demote or suspend the creator and their keys lose power at once.
- *Full access* (`*`) means "everything my creator can do", and is available only to people who have full access themselves.
- Keys can expire (30, 90, 365 days, or never) and can be revoked at any time. Expired, revoked and unknown keys get `401`.
- Keys are managed only in the admin, never through the API, so a leaked key cannot mint new ones.

## Requests

```sh
curl https://your-site.example/api/v1/me \
  -H "Authorization: Bearer ncms_xxxxxxxx_…"
```

- Base URL: `https://<your-site>/api/v1`.
- Send bodies as JSON (`Content-Type: application/json`). Form encodings and `text/plain` are refused by the framework's cross-site protection.
- Uploads (`POST /media?filename=photo.jpg`) send the file bytes as the body with the file's own content type, e.g. `image/jpeg`.
- Cookies are ignored: a browser session can never be used against this API.
- CORS is open (`*`) because authentication is the bearer key, not a cookie. Never put a key in public browser code.
- Rate limit: 600 requests per minute per key (`429` above it).

## Responses

Success:

```json
{ "data": { … }, "meta": { "total": 42, "limit": 50, "offset": 0 } }
```

`meta` appears on lists. Create endpoints answer `201`.

Errors:

```json
{ "error": { "status": 400, "message": "Invalid input", "issues": [{ "field": "title", "message": "Title is required" }] } }
```

| Status | Meaning |
|---|---|
| 400 | Invalid input (`issues` lists the fields) |
| 401 | No key, or an invalid, expired or revoked key |
| 403 | The key lacks the capability the endpoint needs, or a rule forbids it (e.g. only owners change owners) |
| 404 | Unknown endpoint or record |
| 405 | Wrong method for the path |
| 409 | Conflict (e.g. a media item still in use, an email already taken) |
| 413 | Upload larger than 10 MB |
| 429 | Rate limit |

## Content

Content types and their fields come from `GET /types`. Field values go in `data` by field name, with the same types the editor uses: text as strings, `boolean` as `true` / `false`, `tags` and `lines` as arrays of strings, `image` / `reference` / `term` as ids, `users` as an array of user ids, and `blocks` as an array of `{ "type": "hero", "props": { … } }`.

```sh
# Create a draft article
curl -X POST https://your-site.example/api/v1/content/article \
  -H "Authorization: Bearer $KEY" -H "Content-Type: application/json" \
  -d '{ "data": { "title": "Hello", "summary": "One line.", "body": "## Hi\n\nText", "tags": ["news"] } }'

# Schedule it
curl -X POST https://your-site.example/api/v1/content/article/12/status \
  -H "Authorization: Bearer $KEY" -H "Content-Type: application/json" \
  -d '{ "status": "published", "publishAt": "2026-11-01T09:00:00Z" }'

# Change one field (the rest is kept)
curl -X PATCH https://your-site.example/api/v1/content/article/12 \
  -H "Authorization: Bearer $KEY" -H "Content-Type: application/json" \
  -d '{ "data": { "summary": "A better summary." } }'
```

The same rules as the admin apply: authors (`content.edit_own`) change only their own unpublished entries, publishing needs `content.publish`, and renaming a published entry's slug adds a 301 redirect.

## Languages and localisations

Entries are per language (`locale`). `POST /content/:type` takes `locale` (default: the primary localisation's default language) and `markets` (localisation codes; empty = everywhere the language is offered). `GET /content/:type?locale=de` lists one language. `GET /content/:type/:id/translations` shows every language's version (and whether it is behind its source); `POST /content/:type/:id/translations` with `{ "locale": "de" }` creates a linked draft copied from the source, with `data` to supply translated values or `"ai": true` to have Claude translate it (needs the `ANTHROPIC_API_KEY` secret). `/localisations`, `/languages` and `/texts/:code` manage the localisation setup and the interface texts; `PATCH /settings/:group?lang=de` sets a group's translations.

```sh
curl -X POST https://your-site.example/api/v1/content/article/12/translations \
  -H "Authorization: Bearer $KEY" -H "Content-Type: application/json" \
  -d '{ "locale": "de", "ai": true }'
```

## Forms

`GET /forms` lists the forms (fields, settings, translations, unread counts); `POST /forms` and `PATCH /forms/:id` create and change them. Submissions: `GET /submissions?form=&status=&q=`, `GET /submissions/:id`, `PATCH /submissions/:id` with `{ "status": "read" | "archived" | "spam" | "new" }`, `DELETE /submissions/:id`. `POST /forms/:slug/submissions` submits a form on someone's behalf, validated and notified like a visitor's submission. Visitors' browsers post to `/forms/<slug>` on the site itself (not the API).

## Media

```sh
curl -X POST "https://your-site.example/api/v1/media?filename=bike.jpg&alt=A%20red%20bicycle" \
  -H "Authorization: Bearer $KEY" -H "Content-Type: image/jpeg" --data-binary @bike.jpg
```

Use the returned `id` in image fields, or in Markdown as `![alt](media:ID)`.

## Settings

`GET /settings` lists the groups the key may change, with their fields. `PATCH /settings/<group>` merges the values you send and validates the result.

## Two-factor sign-in

`GET /users/{id}/2fa` tells whether a user has it on, how many recovery codes are left and what the policy asks of them; `POST /users/{id}/2fa/reset` removes it (a lost phone). Turning it on needs the person's authenticator app, so setup happens only in the admin profile and the members' account page. The staff and member policies are the `twofactor` settings group (`PATCH /settings/twofactor` with `staff: off | admins | staff`, `members: off | optional | required`). User objects no longer carry `totp_enabled`.

## Form protection

`GET /protection` shows the Turnstile setup (site key, whether a secret is set, never the secret); `PATCH /protection` sets `enabled`, `siteKey`, `secretKey` (write-only; checked with Cloudflare, stored encrypted), `clearSecret` and `appearance`. `GET /protection/forms` lists every registered public form with its switches and 30-day outcome counts; `PATCH /protection/forms/{id}` sets `turnstile` (`default | on | off`) and `honeypot`. Submissions through the API (`POST /forms/{slug}/submissions`) are not subject to these checks: the API key is the proof.

## For developers of this CMS

Every feature registers its endpoints from its own `server/api.c` with `registerEndpoint` (see `src/lib/norns/api/server/router.js`). The router, this reference and the OpenAPI document all read that registry, so a new endpoint documents itself. Anything new in the admin must get an endpoint too (see `CLAUDE.md`).
