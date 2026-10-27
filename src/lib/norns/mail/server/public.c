import type { Container } from '@human-synthesis/norns/server'
import type { MailService, Message } from './service'

export type { Message }

svc := (c: Container) => c.resolve('mail.service') as MailService

export mail := (c: Container) => {
	send: (msg: Message) => svc(c).send(msg)
	provider: () => svc(c).provider()
	log: (limit?: number) => svc(c).log(limit)
	lastTo: (email: string) => svc(c).lastTo(email)
	purge: (before: number) => svc(c).purge(before)
	notifyTo: async () => ((await (c.resolve('settings.service') as any).get('mail')).notifyTo ?? []) as string[]
}
