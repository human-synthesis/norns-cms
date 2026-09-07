import type { Container } from '@human-synthesis/norns/server'
import { registerType } from '../../content/server/public'
import { pageType } from '../shared/types'

export default (app: Container) =>
	registerType app, pageType
