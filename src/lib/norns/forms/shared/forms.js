// Form definitions, shared by the server (validation, notifications) and the
// UI (rendering, the admin field builder).

export const FIELD_TYPES = [
	{ id: 'text', label: 'Text' },
	{ id: 'email', label: 'Email' },
	{ id: 'tel', label: 'Phone' },
	{ id: 'url', label: 'Website' },
	{ id: 'number', label: 'Number' },
	{ id: 'date', label: 'Date' },
	{ id: 'textarea', label: 'Long text' },
	{ id: 'select', label: 'Dropdown' },
	{ id: 'radio', label: 'Choice (one)' },
	{ id: 'checkboxes', label: 'Choice (several)' },
	{ id: 'checkbox', label: 'Checkbox' },
	{ id: 'consent', label: 'Consent checkbox' }
];
export const TYPE_IDS = FIELD_TYPES.map((t) => t.id);
export const WITH_OPTIONS = new Set(['select', 'radio', 'checkboxes']);

/**
 * A form as one language sees it: labels, placeholders, help, options and
 * messages from translations[lang] (then its fallbacks), else the base.
 * `chain` is the language followed by its fallbacks.
 */
export function localizeForm(form, chain = []) {
	const pick = (get) => {
		for (const code of chain) {
			const v = get(form.translations?.[code] ?? {});
			if (v != null && v !== '') return v;
		}
		return undefined;
	};
	const st = form.settings ?? {};
	return {
		id: form.id,
		slug: form.slug,
		kind: form.kind,
		name: pick((t) => t.name) ?? form.name,
		submitLabel: pick((t) => t.submitLabel) ?? st.submitLabel ?? '',
		successMessage: pick((t) => t.successMessage) ?? st.successMessage ?? '',
		duplicateMessage: pick((t) => t.duplicateMessage) ?? st.duplicateMessage ?? '',
		fields: (form.fields ?? []).map((f) => ({
			...f,
			label: pick((t) => t.fields?.[f.name]?.label) ?? f.label,
			placeholder: pick((t) => t.fields?.[f.name]?.placeholder) ?? f.placeholder ?? '',
			help: pick((t) => t.fields?.[f.name]?.help) ?? f.help ?? '',
			options: (f.options ?? []).map((o, i) => ({ value: o.value, label: pick((t) => t.fields?.[f.name]?.options?.[i]) ?? o.label }))
		}))
	};
}

/** "Name: Ada\nEmail: …" for notifications and exports. */
export function describeSubmission(fields, data) {
	return fields
		.map((f) => {
			const v = data[f.name];
			const shown = Array.isArray(v) ? v.join(', ') : typeof v === 'boolean' ? (v ? 'yes' : 'no') : String(v ?? '');
			return `${f.label}: ${shown}`;
		})
		.join('\n');
}
