// Interface texts and formatting in components. The site layout puts the
// request's language and merged strings on page.data.i18n.
import { page } from '$app/state';
import { translate } from '../shared/t.js';

export const t = (key, vars) => translate(page.data?.i18n?.strings, key, vars);

export const lang = () => page.data?.i18n?.locale ?? 'en';

export const fmtDate = (d, opts = { year: 'numeric', month: 'long', day: 'numeric' }) => {
	try {
		return new Date(d).toLocaleDateString(lang(), opts);
	} catch {
		return new Date(d).toLocaleDateString('en', opts);
	}
};
