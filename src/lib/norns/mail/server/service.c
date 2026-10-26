import { envVar } from '../../settings/server/public'

export type Message =
	to: string
	subject: string
	text: string
	html?: string
	template?: string

escape := (s: string) => s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')

// Plain text → a minimal, client-safe HTML body (links become anchors).
export toHtml := (text: string) =>
	paragraphs := escape(text).split(/\n{2,}/).map (p) =>
		linked := p.replace(/(https?:\/\/[^\s<]+)/g, '<a href="$1">$1</a>').replace(/\n/g, '<br>')
		`<p style="margin:0 0 16px">${linked}</p>`
	`<div style="font-family:system-ui,sans-serif;font-size:15px;line-height:1.5;color:#1f2937;max-width:560px">${paragraphs.join('')}</div>`

// Outgoing mail over an HTTP API (Workers cannot speak SMTP). With
// RESEND_API_KEY set, mail goes through Resend from MAIL_FROM; without it
// the message is written to mail_log (body included) and the console, which
// is how dev and tests "receive" invites and reset links.
export class MailService
	db: any
	constructor(@db: any)

	provider(): string
		envVar('RESEND_API_KEY') ? 'resend' : 'log'

	async send(msg: Message): Promise<{ ok: boolean, provider: string, error?: string }>
		provider := @provider()
		html := msg.html ?? toHtml(msg.text)
		if provider === 'log'
			console.log `[mail] to=${msg.to} subject=${JSON.stringify(msg.subject)}\n${msg.text}`
			await @record msg, provider, 'logged', msg.text, ''
			return { ok: true, provider }
		try
			payload := { from: envVar('MAIL_FROM') ?? 'Norns CMS <no-reply@humansynthesis.ai>', to: [msg.to], subject: msg.subject, text: msg.text, html }
			headers := { 'Authorization': `Bearer ${envVar('RESEND_API_KEY')}`, 'Content-Type': 'application/json' }
			res := await fetch('https://api.resend.com/emails', { method: 'POST', headers, body: JSON.stringify(payload) })
			if res.ok
				await @record msg, provider, 'sent', '', ''
				return { ok: true, provider }
			detail := (await res.text()).slice(0, 300)
			await @record msg, provider, 'failed', '', `${res.status} ${detail}`
			{ ok: false, provider, error: `Mail provider answered ${res.status}` }
		catch e
			await @record msg, provider, 'failed', '', String(e).slice(0, 300)
			{ ok: false, provider, error: 'Mail provider unreachable' }

	async record(msg: Message, provider: string, status: string, body: string, error: string)
		try
			await @db.run 'INSERT INTO mail_log (recipient, subject, template, provider, status, body, error, created_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?)', [msg.to, msg.subject, msg.template ?? '', provider, status, body, error, Date.now()]
		catch
			// mail_log missing (migrations not applied); sending still counts.
			return
		return

	async log(limit = 100): Promise<any[]>
		await @db.all 'SELECT * FROM mail_log ORDER BY id DESC LIMIT ?', [limit]

	async lastTo(email: string): Promise<any>
		await @db.get 'SELECT * FROM mail_log WHERE recipient = ? ORDER BY id DESC LIMIT 1', [email]

	async purge(before: number): Promise<number>
		r := await @db.run 'DELETE FROM mail_log WHERE created_at < ?', [before]
		r.changes
