// Comments: the site-wide settings, the per-entry switch (a field group on
// every type that allows comments) and the pure helpers shared by the
// server and the theme.

export commentsSettings := {
	id: 'comments'
	label: 'Comments'
	icon: 'lucide:message-square'
	order: 40
	capability: 'comments.moderate'
	description: 'Where visitors can comment and reply, who may write, and what waits for approval.'
	fields: [
		{ name: 'enabled', label: 'Comments on the site', type: 'boolean', default: true, help: 'Off hides every comment section; nothing is deleted' }
		{ name: 'types', label: 'Comments on', type: 'checks', optionsFrom: 'comments:types', default: ['article'], help: 'Content types with comments open by default. Each entry can still open or close them (Settings tab of the editor)' }
		{ name: 'who', label: 'Who can comment', type: 'select', default: 'members', options: [
			{ value: 'members', label: 'Signed-in members (and staff)' }
			{ value: 'anyone', label: 'Anyone (visitors give a name and email)' }
		] }
		{ name: 'moderation', label: 'Approval', type: 'select', default: 'guests', options: [
			{ value: 'all', label: 'Every comment waits for approval' }
			{ value: 'guests', label: "Visitors' comments wait; members' appear at once" }
			{ value: 'none', label: 'Comments appear at once' }
		] }
		{ name: 'replies', label: 'Allow replies', type: 'boolean', default: true }
		{ name: 'maxDepth', label: 'Reply levels', type: 'number', default: 3, help: 'How deep replies can nest (1–5)' }
		{ name: 'closeAfterDays', label: 'Close comments after (days)', type: 'number', default: 0, help: 'Days after publishing; 0 keeps them open' }
		{ name: 'maxLinks', label: 'Links allowed per comment', type: 'number', default: 2, help: 'More links send the comment to Spam' }
		{ name: 'notify', label: 'Email the team about new comments', type: 'boolean', default: true, help: 'To Settings → Email → Notification address' }
	]
}

export commentsFieldGroup := {
	id: 'comments'
	label: 'Comments'
	tab: 'details'
	appliesTo: (ct: any) => ct.comments !== false
	fields: [
		{ name: 'commentsMode', label: 'Comments', type: 'select', default: '', options: [
			{ value: '', label: 'As set for this type (Settings → Comments)' }
			{ value: 'open', label: 'Open' }
			{ value: 'closed', label: 'Closed: show comments, take no new ones' }
			{ value: 'off', label: 'Off: no comment section' }
		] }
	]
}

export STATUSES := ['pending', 'approved', 'spam', 'trash']

// Plain text → safe HTML: escaped, paragraphs and line breaks, http(s)
// links as rel="nofollow ugc". Links are found in the raw text, then each
// piece is escaped on its own.
URL_RE := /https?:\/\/[^\s<>"']+[^\s<>"'.,;:!?)\]]/g

esc := (s: string) => s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;')

linked := (text: string) =>
	out .= ''
	last .= 0
	for m of text.matchAll(URL_RE)
		out += esc(text.slice(last, m.index)) + `<a href="${esc(m[0])}" rel="nofollow ugc noopener" target="_blank">${esc(m[0])}</a>`
		last = m.index + m[0].length
	out + esc(text.slice(last))

export commentHtml := (text: string) =>
	String(text ?? '').trim().split(/\n{2,}/).map((p) => `<p>${linked(p).replace(/\n/g, '<br>')}</p>`).join('')
