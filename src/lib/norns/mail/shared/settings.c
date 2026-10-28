// Settings → Email: who mail comes from and where form notifications go.
export mailSettings := {
	id: 'mail'
	label: 'Email'
	icon: 'lucide:mail'
	order: 40
	description: 'Mail is sent through Aiosis Emails and needs the AIOSIS_API_KEY secret (scope messages:send). Each kind of mail has its own sender: signup@ for invitations and address verification, signin@ for password resets, notification@ for team notifications and form submissions, contact@ for contact form submissions — on the mail domain unless a full address is set below.'
	fields: [
		{ name: 'fromName', label: 'Sender name', type: 'text', default: 'Norns CMS', help: 'Defaults to the site title when empty' }
		{ name: 'domain', label: 'Mail domain', type: 'text', help: 'A domain set up in Aiosis Emails, e.g. example.com. Empty: the MAIL_DOMAIN variable, else the site\'s own domain' }
		{ name: 'signupFrom', label: 'Sign-up sender', type: 'email', help: 'Invitations, address verification. Empty: signup@ on the mail domain' }
		{ name: 'signinFrom', label: 'Sign-in sender', type: 'email', help: 'Password resets. Empty: signin@ on the mail domain' }
		{ name: 'notificationsFrom', label: 'Notification sender', type: 'email', help: 'Review requests, form submissions. Empty: notification@ on the mail domain' }
		{ name: 'contactFrom', label: 'Contact sender', type: 'email', help: 'Contact form submissions. Empty: contact@ on the mail domain' }
		{ name: 'replyTo', label: 'Reply-to address', type: 'email', help: 'Where replies to system mail go (form notifications reply to the person who wrote)' }
		{ name: 'notifyTo', label: 'Notification address', type: 'lines', help: 'One address per line. Receives form submissions unless a form names its own. Aiosis delivers only to addresses verified on the account.' }
	]
}
