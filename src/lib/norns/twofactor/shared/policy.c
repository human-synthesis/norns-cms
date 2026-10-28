// Two audiences, two policies. CMS staff sign in to the admin; members are
// the people who use the site's account area and the apps built on it.
// Neither policy touches the other.

export twoFactorSettings := {
	id: 'twofactor'
	label: 'Two-factor sign-in'
	icon: 'lucide:shield-check'
	order: 91
	description: 'Who must use an authenticator app on top of their password: CMS staff and members are set separately.'
	fields: [
		{ name: 'staff', label: 'CMS staff (admin area)', type: 'select', default: 'off', required: true, options: [
			{ value: 'off', label: 'Optional — each person decides on their profile' }
			{ value: 'admins', label: 'Required for owners and administrators' }
			{ value: 'staff', label: 'Required for all staff' }
		], help: 'Staff who must use it and have not set it up can only reach their profile until they do.' }
		{ name: 'members', label: 'Members (account area and apps)', type: 'select', default: 'off', required: true, options: [
			{ value: 'off', label: 'Off — not offered to members' }
			{ value: 'optional', label: 'Optional — members can turn it on in their account' }
			{ value: 'required', label: 'Required — members set it up before using member features' }
		], help: 'A member who must set it up keeps their account page but loses member-only content and app access until they do.' }
	]
}

ADMIN_ROLES := ['owner', 'admin']

// What the policy means for one user: 'required', 'optional' or 'off'
// (not offered; a user who already has it keeps it).
export policyFor := (values: any, user: any): string =>
	return 'off' unless user
	if user.kind === 'staff'
		staff := values?.staff ?? 'off'
		return 'required' if staff === 'staff' || (staff === 'admins' && ADMIN_ROLES.includes(user.role))
		return 'optional'
	members := values?.members ?? 'off'
	return members if ['optional', 'required'].includes(members)
	'off'
