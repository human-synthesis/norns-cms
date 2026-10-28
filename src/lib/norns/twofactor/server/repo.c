// One row per user in two_factor; see migrations/twofactor.
export type TwoFactorRow =
	user_id: number
	secret: string
	enabled_at: number | null
	recovery_codes: string
	updated_at: number

export class TwoFactorRepo
	db: any
	constructor(@db: any)

	get(userId: number): Promise<TwoFactorRow | undefined>
		@db.get 'SELECT * FROM two_factor WHERE user_id = ?', [userId]

	async enabledIds(ids: number[]): Promise<number[]>
		return [] unless ids.length
		rows := await @db.all `SELECT user_id FROM two_factor WHERE enabled_at IS NOT NULL AND user_id IN (${ids.map(() => '?').join(',')})`, ids
		rows.map (r: any) => r.user_id

	async start(userId: number, secret: string)
		await @db.run 'INSERT INTO two_factor (user_id, secret, enabled_at, recovery_codes, updated_at) VALUES (?, ?, NULL, ?, ?) ON CONFLICT(user_id) DO UPDATE SET secret = excluded.secret, enabled_at = NULL, recovery_codes = excluded.recovery_codes, updated_at = excluded.updated_at', [userId, secret, '[]', Date.now()]
		return

	async enable(userId: number, codes: string[])
		await @db.run 'UPDATE two_factor SET enabled_at = ?, recovery_codes = ?, updated_at = ? WHERE user_id = ?', [Date.now(), JSON.stringify(codes), Date.now(), userId]
		return

	async setCodes(userId: number, codes: string[])
		await @db.run 'UPDATE two_factor SET recovery_codes = ?, updated_at = ? WHERE user_id = ?', [JSON.stringify(codes), Date.now(), userId]
		return

	async remove(userId: number)
		await @db.run 'DELETE FROM two_factor WHERE user_id = ?', [userId]
		return
