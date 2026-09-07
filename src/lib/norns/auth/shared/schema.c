import * as v from 'valibot'

export loginSchema := v.object
	email: v.pipe v.string('email is required'), v.trim(), v.email('enter a valid email')
	password: v.pipe v.string('password is required'), v.minLength(1, 'password is required')
	next: v.optional v.string(), '/admin'

export type LoginInput = v.InferOutput<typeof loginSchema>
