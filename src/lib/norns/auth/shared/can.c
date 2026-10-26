// Capability check, pure so any feature (and the admin UI) can use it.
// A user carries `capabilities` (resolved from the role at session time):
// '*' grants everything, 'content.*' grants every 'content.' capability.
export capMatches := (granted: string[], cap: string) =>
	for g of granted
		return true if g === '*' || g === cap
		return true if g.endsWith('.*') && cap.startsWith(g.slice(0, -1))
	false

export can := (user: any, cap: string) =>
	return false unless user && user.status !== 'suspended' && user.status !== 'deleted'
	capMatches user.capabilities ?? [], cap

export isStaff := (user: any) =>
	!!user && user.kind === 'staff' && can(user, 'admin.access')
