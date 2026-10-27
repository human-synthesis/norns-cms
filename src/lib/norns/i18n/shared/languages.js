// Languages offered in the admin's picker (any BCP 47 code can be added by
// hand). name = English, native = the language's own name.
export const KNOWN_LANGUAGES = [
	['ar', 'Arabic', 'العربية', 'rtl'],
	['bg', 'Bulgarian', 'Български'],
	['ca', 'Catalan', 'Català'],
	['cs', 'Czech', 'Čeština'],
	['da', 'Danish', 'Dansk'],
	['de', 'German', 'Deutsch'],
	['de-AT', 'German (Austria)', 'Deutsch (Österreich)'],
	['de-CH', 'German (Switzerland)', 'Deutsch (Schweiz)'],
	['el', 'Greek', 'Ελληνικά'],
	['en', 'English', 'English'],
	['en-GB', 'English (UK)', 'English (UK)'],
	['en-US', 'English (US)', 'English (US)'],
	['es', 'Spanish', 'Español'],
	['es-MX', 'Spanish (Mexico)', 'Español (México)'],
	['et', 'Estonian', 'Eesti'],
	['fa', 'Persian', 'فارسی', 'rtl'],
	['fi', 'Finnish', 'Suomi'],
	['fr', 'French', 'Français'],
	['fr-CA', 'French (Canada)', 'Français (Canada)'],
	['fr-CH', 'French (Switzerland)', 'Français (Suisse)'],
	['he', 'Hebrew', 'עברית', 'rtl'],
	['hi', 'Hindi', 'हिन्दी'],
	['hr', 'Croatian', 'Hrvatski'],
	['hu', 'Hungarian', 'Magyar'],
	['id', 'Indonesian', 'Bahasa Indonesia'],
	['it', 'Italian', 'Italiano'],
	['it-CH', 'Italian (Switzerland)', 'Italiano (Svizzera)'],
	['ja', 'Japanese', '日本語'],
	['ko', 'Korean', '한국어'],
	['lt', 'Lithuanian', 'Lietuvių'],
	['lv', 'Latvian', 'Latviešu'],
	['nb', 'Norwegian', 'Norsk bokmål'],
	['nl', 'Dutch', 'Nederlands'],
	['nl-BE', 'Dutch (Belgium)', 'Nederlands (België)'],
	['pl', 'Polish', 'Polski'],
	['pt', 'Portuguese', 'Português'],
	['pt-BR', 'Portuguese (Brazil)', 'Português (Brasil)'],
	['ro', 'Romanian', 'Română'],
	['ru', 'Russian', 'Русский'],
	['sk', 'Slovak', 'Slovenčina'],
	['sl', 'Slovenian', 'Slovenščina'],
	['sr', 'Serbian', 'Српски'],
	['sv', 'Swedish', 'Svenska'],
	['th', 'Thai', 'ไทย'],
	['tr', 'Turkish', 'Türkçe'],
	['uk', 'Ukrainian', 'Українська'],
	['ur', 'Urdu', 'اردو', 'rtl'],
	['vi', 'Vietnamese', 'Tiếng Việt'],
	['zh', 'Chinese (Simplified)', '简体中文'],
	['zh-TW', 'Chinese (Traditional)', '繁體中文']
].map(([code, name, native, dir = 'ltr']) => ({ code, name, native, dir }));

export const RTL = new Set(['ar', 'he', 'fa', 'ur']);
export const dirOf = (code) => (RTL.has(String(code).split('-')[0]) ? 'rtl' : 'ltr');

/** Base language of a regional code: de-CH → de. */
export const baseLanguage = (code) => String(code).split('-')[0];

/** Default URL segment for a language code: de-CH → de-ch. */
export const defaultSegment = (code) => String(code).toLowerCase();

export const LANGUAGE_RE = /^[a-z]{2,3}(-[A-Za-z0-9]{2,8})*$/;
