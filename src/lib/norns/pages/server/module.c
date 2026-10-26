import type { Container } from '@human-synthesis/norns/server'
import { registerType, registerBlock, embedHtml } from '../../content/server/public'
import { pageType } from '../shared/types'
import { pageBlocks, embedBlock } from '../shared/blocks'

// YouTube / Vimeo links → their privacy-friendly embed; anything else https
// embeds as is. Same consent-aware placeholder as the ::embed directive.
embedFor := (url: string, title: string) =>
	yt := /(?:youtube\.com\/(?:watch\?v=|embed\/|shorts\/)|youtu\.be\/)([\w-]{6,20})/.exec(url)
	return embedHtml('youtube', { id: yt[1], title }) if yt
	vm := /vimeo\.com\/(?:video\/)?(\d{4,15})/.exec(url)
	return embedHtml('vimeo', { id: vm[1], title }) if vm
	embedHtml 'embed', { url, title }

export default (app: Container) =>
	registerType app, pageType
	for block of pageBlocks
		if block.id === 'embed'
			registerBlock app, { ...embedBlock, resolve: (props: any) => ({ html: embedFor(String(props.url ?? ''), String(props.title ?? '')) }) }
		else
			registerBlock app, block
