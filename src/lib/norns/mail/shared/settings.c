// Settings → Email: who mail comes from and where form notifications go.
export mailSettings := {
	id: 'mail'
	label: 'Email'
	icon: 'lucide:mail'
	order: 40
	description: 'Mail is sent through Aiosis Emails and needs the AIOSIS_API_KEY secret (scope messages:send). Each kind of mail has its own sender on the mail domain: signup@ for invitations and address verification, signin@ for password resets, notifications@ for team notifications and form submissions, contact@ for contact form submissions.'
	fields: [
		{ name: 'fromName', label: 'Sender name', type: 'text', default: 'Norns CMS', help: 'Defaults to the site title when empty' }
		{ name: 'domain', label: 'Mail domain', type: 'text', help: 'A domain set up in Aiosis Emails, e.g. example.com. Empty: the MAIL_DOMAIN variable, else the site\'s own domain' }
		{ name: 'replyTo', label: 'Reply-to address', type: 'email', help: 'Where replies to system mail go (form notifications reply to the person who wrote)' }
		{ name: 'notifyTo', label: 'Notification address', type: 'lines', help: 'One address per line. Receives form submissions unless a form names its own. Aiosis delivers only to addresses verified on the account.' }
	]
}
