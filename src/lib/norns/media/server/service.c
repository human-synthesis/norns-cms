import { randomBytes } from 'node:crypto'
import type { MediaRepo, Media } from './repo'
import { r2Storage, dirStorage, imageSize, sniffMime } from './storage.js'
import { binding, envVar } from '../../settings/server/public'

export MAX_BYTES := 10 * 1024 * 1024
export ALLOWED := ['image/png', 'image/jpeg', 'image/gif', 'image/webp', 'image/avif', 'application/pdf']

cleanName := (name: string) =>
	base := name.split(/[\\/]/).pop() ?? 'file'
	base.normalize('NFKD').replace(/[^\w.-]+/g, '-').replace(/-+/g, '-').replace(/^-|-$/g, '').slice(0, 80) || 'file'

EXT := { 'image/png': 'png', 'image/jpeg': 'jpg', 'image/gif': 'gif', 'image/webp': 'webp', 'image/avif': 'avif', 'application/pdf': 'pdf' } as Record<string, string>

export class MediaService
	repo: MediaRepo
	resizing: boolean
	constructor(@repo: MediaRepo, @resizing = false)

	storage()
		bucket := binding 'MEDIA'
		bucket ? r2Storage(bucket) : dirStorage(envVar('MEDIA_DIR') ?? 'data/media')

	url(m: Media | { key: string }): string
		`/media/${m.key}`

	// Responsive variants through Cloudflare Image Resizing when enabled.
	variant(m: Media, width: number): string
		return @url(m) unless @resizing && m.mime.startsWith('image/') && m.mime !== 'image/gif'
		`/cdn-cgi/image/width=${width},quality=82,format=auto${@url(m)}`

	present(m: Media)
		isImage := m.mime.startsWith('image/')
		widths := [480, 960, 1440, 1920].filter((w) => !m.width || w < m.width)
		srcset := isImage && @resizing && widths.length ? [...widths.map((w) => `${@variant(m, w)} ${w}w`), ...(m.width ? [`${@url(m)} ${m.width}w`] : [])].join(', ') : ''
		{ id: m.id, url: @url(m), thumb: isImage ? @variant(m, 480) : '', srcset, width: m.width, height: m.height, alt: m.alt, caption: m.caption, mime: m.mime, filename: m.filename, size: m.size }

	async upload(file: File, meta: { alt?: string, caption?: string, authorId?: number | null } = {})
		return { ok: false, error: 'Choose a file' } unless file && file.size
		return { ok: false, error: `${file.name} is larger than 10 MB` } if file.size > MAX_BYTES
		bytes := new Uint8Array(await file.arrayBuffer())
		mime := sniffMime bytes
		return { ok: false, error: `${file.name}: only PNG, JPEG, GIF, WebP, AVIF and PDF files are allowed` } unless mime && ALLOWED.includes(mime)
		dims := mime.startsWith('image/') ? imageSize(bytes) : null
		name := cleanName file.name
		stem := name.replace(/\.[^.]+$/, '') || 'file'
		d := new Date()
		key := `${d.getUTCFullYear()}/${String(d.getUTCMonth() + 1).padStart(2, '0')}/${randomBytes(6).toString('hex')}-${stem}.${EXT[mime]}`
		await @storage().put key, bytes, mime
		id := await @repo.insert { key, filename: name, mime, size: bytes.length, width: dims?.width ?? null, height: dims?.height ?? null, alt: meta.alt ?? '', caption: meta.caption ?? '', author_id: meta.authorId ?? null }
		{ ok: true, media: @present((await @repo.get id) as Media) }

	async read(key: string)
		m := await @repo.byKey key
		return null unless m
		obj := await @storage().get key, m.mime
		obj ? { ...obj, mime: m.mime, filename: m.filename } : null

	async get(id: number)
		m := await @repo.get id
		m ? @present(m) : undefined

	async resolve(ids: number[]): Promise<Record<string, any>>
		out := {} as Record<string, any>
		for m of await @repo.byIds(ids)
			out[m.id] = @present(m)
		out

	async list(q: any = {})
		rows := await @repo.list q
		rows.map((m: any) => ({ ...@present(m), usage: Number(m.usage ?? 0), created_at: m.created_at }))

	count()
		@repo.count()

	update(id: number, fields: { alt?: string, caption?: string, filename?: string })
		@repo.update id, fields

	usage(id: number)
		@repo.usage id

	async remove(id: number, force = false)
		m := await @repo.get id
		return { ok: false, error: 'Not found' } unless m
		used := await @repo.usage id
		return { ok: false, error: `Still used by ${used.length} entr${used.length === 1 ? 'y' : 'ies'}`, usage: used } if used.length && !force
		await @storage().delete m.key
		await @repo.remove id
		{ ok: true }

	setUsage(entryId: number, ids: number[])
		@repo.setUsage entryId, ids
