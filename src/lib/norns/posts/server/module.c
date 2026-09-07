import type { Container } from '@human-synthesis/norns/server'
import { registerType } from '../../content/server/public'
import { postType } from '../shared/types'

export default (app: Container) =>
	registerType app, postType
