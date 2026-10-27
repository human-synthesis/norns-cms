import { envVar } from '../../settings/server/public'

export type Message =
	to: string
	subject: string
	text: string
	html?: string
	template?: string
	replyTo?: string
	fromName?: string

export type Sender =
	fromEmail: string
	fromName: string
	replyTo: string

escape := (s: string) => s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')

// Plain text → a minimal, client-safe HTML body (links become anchors).
export toHtml := (text: string) =>
	paragraphs := escape(text).split(/\n{2,}/).map (p) =>
		linked := p.replace(/(https?:\/\/[^\s<]+)/g, '<a href="$1">$1</a>').replace(/\n/g, '<br>')
		`<p style="margin:0 0 16px">${linked}</p>`
	`<div style="font-family:system-ui,sans-serif;font-size:15px;line-height:1.5;color:#1f2937;max-width:560px">${paragraphs.join('')}</div>`

API := => (envVar('AIOSIS_API_URL') ?? 'https://emails.aiosis.com/api/v1').replace(/\/+$/, '')

// Outgoing mail over Aiosis Emails' HTTP API (Workers cannot speak SMTP):
// POST /messages with a key holding the `messages:send` scope. Without
// AIOSIS_API_KEY the message is only written to mail_log (body included)
// and the console, which is how dev and tests "receive" mail.
//
// Aiosis delivers only to recipient addresses verified on its account (a
// first message to any other address is refused with a 400), so it suits
// notifications to the site's own team; such refusals are logged as
// failed with the provider's message.
export class MailService
	db: any
	sender: () => Promise<Sender>
	constructor(@db: any, @sender: () => Promise<Sender>)

	provider(): string
		envVar('AIOSIS_API_KEY') ? 'aiosis' : 'log'

	async send(msg: Message): Promise<{ ok: boolean, provider: string, error?: string, id?: string }>
		provider := @provider()
		html := msg.html ?? toHtml(msg.text)
		if provider === 'log'
			console.log `[mail] to=${msg.to} subject=${JSON.stringify(msg.subject)}\n${msg.text}`
			await @record msg, provider, 'logged', msg.text, ''
			return { ok: true, provider }
		from := await @sender()
		payload := { to: msg.to, subject: msg.subject, text: msg.text, html, fromEmail: from.fromEmail, fromName: msg.fromName || from.fromName } as Record<string, any>
		replyTo := msg.replyTo || from.replyTo
		payload.replyTo = replyTo if replyTo
		try
			headers := { 'Authorization': `Bearer ${envVar('AIOSIS_API_KEY')}`, 'Content-Type': 'application/json' }
			res := await fetch(`${API()}/messages`, { method: 'POST', headers, body: JSON.stringify(payload) })
			body := await res.json().catch(=> ({}))
			if res.ok
				await @record msg, provider, 'sent', '', '', body.id ?? ''
				return { ok: true, provider, id: body.id }
			detail := String(body.message ?? `HTTP ${res.status}`).slice(0, 300)
			await @record msg, provider, 'failed', '', `${res.status} ${detail}`
			{ ok: false, provider, error: detail }
		catch e
			await @record msg, provider, 'failed', '', String(e).slice(0, 300)
			{ ok: false, provider, error: 'Mail provider unreachable' }

	async record(msg: Message, provider: string, status: string, body: string, error: string, providerId = '')
		try
			await @db.run 'INSERT INTO mail_log (recipient, subject, template, provider, status, body, error, created_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?)', [msg.to, msg.subject, msg.template ?? '', provider, status, body, error || (providerId ? `id ${providerId}` : ''), Date.now()]
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
