// Settings → Email: who mail comes from and where form notifications go.
export mailSettings := {
	id: 'mail'
	label: 'Email'
	icon: 'lucide:mail'
	order: 40
	description: 'Sender and notification addresses. Mail is sent through Aiosis Emails when the AIOSIS_API_KEY secret is set; otherwise it is only written to the mail log.'
	fields: [
		{ name: 'fromName', label: 'Sender name', type: 'text', default: 'Norns CMS', help: 'Defaults to the site title when empty' }
		{ name: 'fromEmail', label: 'Sender address', type: 'email', default: 'no-reply@humansynthesis.ai', required: true, help: 'An address on a domain set up in Aiosis Emails' }
		{ name: 'replyTo', label: 'Reply-to address', type: 'email', help: 'Where replies to system mail go (form notifications reply to the person who wrote)' }
		{ name: 'notifyTo', label: 'Notification address', type: 'lines', help: 'One address per line. Receives form submissions unless a form names its own. Aiosis delivers only to addresses verified on the account.' }
	]
}
