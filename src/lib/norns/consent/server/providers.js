// Script presets for the consent manager. Each provider renders its own
// snippet from validated config values (ids are checked against strict
// patterns, so nothing user-typed reaches the page unescaped), declares its
// cookies for the banner and the cookie table, and the patterns used to
// delete them on withdrawal.
const esc = (s) => String(s ?? '').replace(/[&<>"']/g, (ch) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[ch]);
const js = (s) => JSON.stringify(String(s ?? '')).replace(/</g, '\\u003c');

export const PROVIDERS = {
	ga4: {
		label: 'Google Analytics 4',
		category: 'analytics',
		google: true,
		fields: [{ name: 'measurementId', label: 'Measurement ID', placeholder: 'G-XXXXXXXXXX', pattern: '^G-[A-Z0-9]{4,15}$' }],
		cookies: [
			{ name: '_ga', duration: '2 years', purpose: 'Distinguishes visitors (Google Analytics)' },
			{ name: '_ga_<id>', duration: '2 years', purpose: 'Keeps session state (Google Analytics)' }
		],
		patterns: ['_ga', '_ga_*', '_gid', '_gat*'],
		render: (c) => ({
			head: `<script async src="https://www.googletagmanager.com/gtag/js?id=${esc(c.measurementId)}"></script><script>window.dataLayer=window.dataLayer||[];function gtag(){dataLayer.push(arguments);}gtag('js',new Date());gtag('config',${js(c.measurementId)},{anonymize_ip:true});</script>`
		})
	},
	gtm: {
		label: 'Google Tag Manager',
		category: 'analytics',
		google: true,
		fields: [{ name: 'containerId', label: 'Container ID', placeholder: 'GTM-XXXXXXX', pattern: '^GTM-[A-Z0-9]{4,10}$' }],
		cookies: [{ name: 'depends on the tags', duration: '—', purpose: 'Tags configured in Google Tag Manager' }],
		patterns: ['_ga', '_ga_*', '_gid', '_gcl_*'],
		render: (c) => ({
			head: `<script>(function(w,d,s,l,i){w[l]=w[l]||[];w[l].push({'gtm.start':new Date().getTime(),event:'gtm.js'});var f=d.getElementsByTagName(s)[0],j=d.createElement(s),dl=l!='dataLayer'?'&l='+l:'';j.async=true;j.src='https://www.googletagmanager.com/gtm.js?id='+i+dl;var n=d.querySelector('script[nonce]');if(n&&n.nonce)j.nonce=n.nonce;f.parentNode.insertBefore(j,f);})(window,document,'script','dataLayer',${js(c.containerId)});</script>`,
			body: `<noscript><iframe src="https://www.googletagmanager.com/ns.html?id=${esc(c.containerId)}" height="0" width="0" style="display:none;visibility:hidden"></iframe></noscript>`
		})
	},
	plausible: {
		label: 'Plausible Analytics',
		category: 'analytics',
		fields: [
			{ name: 'domain', label: 'Site domain', placeholder: 'example.com', pattern: '^[a-z0-9.-]+$' },
			{ name: 'src', label: 'Script URL', placeholder: 'https://plausible.io/js/script.js', pattern: '^https://[^\\s"\'<>]+$', optional: true }
		],
		cookies: [{ name: '(none)', duration: '—', purpose: 'Plausible sets no cookies' }],
		patterns: [],
		render: (c) => ({ head: `<script defer data-domain="${esc(c.domain)}" src="${esc(c.src || 'https://plausible.io/js/script.js')}"></script>` })
	},
	matomo: {
		label: 'Matomo',
		category: 'analytics',
		fields: [
			{ name: 'url', label: 'Matomo URL', placeholder: 'https://analytics.example.com/', pattern: '^https://[^\\s"\'<>]+/$' },
			{ name: 'siteId', label: 'Site ID', placeholder: '1', pattern: '^\\d{1,6}$' }
		],
		cookies: [
			{ name: '_pk_id.*', duration: '13 months', purpose: 'Distinguishes visitors (Matomo)' },
			{ name: '_pk_ses.*', duration: '30 minutes', purpose: 'Session (Matomo)' }
		],
		patterns: ['_pk_*', 'mtm_*'],
		render: (c) => ({
			head: `<script>var _paq=window._paq=window._paq||[];_paq.push(['trackPageView']);_paq.push(['enableLinkTracking']);(function(){var u=${js(c.url)};_paq.push(['setTrackerUrl',u+'matomo.php']);_paq.push(['setSiteId',${js(c.siteId)}]);var d=document,g=d.createElement('script'),s=d.getElementsByTagName('script')[0];g.async=true;g.src=u+'matomo.js';var n=d.querySelector('script[nonce]');if(n&&n.nonce)g.nonce=n.nonce;s.parentNode.insertBefore(g,s);})();</script>`
		})
	},
	clarity: {
		label: 'Microsoft Clarity',
		category: 'analytics',
		fields: [{ name: 'projectId', label: 'Project ID', placeholder: 'abcd1234ef', pattern: '^[a-z0-9]{6,14}$' }],
		cookies: [
			{ name: '_clck', duration: '1 year', purpose: 'Distinguishes visitors (Clarity)' },
			{ name: '_clsk', duration: '1 day', purpose: 'Joins page views into a session (Clarity)' }
		],
		patterns: ['_clck', '_clsk', 'CLID', 'MUID'],
		render: (c) => ({
			head: `<script>(function(c,l,a,r,i,t,y){c[a]=c[a]||function(){(c[a].q=c[a].q||[]).push(arguments)};t=l.createElement(r);t.async=1;t.src="https://www.clarity.ms/tag/"+i;var n=l.querySelector('script[nonce]');if(n&&n.nonce)t.nonce=n.nonce;y=l.getElementsByTagName(r)[0];y.parentNode.insertBefore(t,y);})(window,document,"clarity","script",${js(c.projectId)});</script>`
		})
	},
	hotjar: {
		label: 'Hotjar',
		category: 'analytics',
		fields: [{ name: 'siteId', label: 'Site ID', placeholder: '1234567', pattern: '^\\d{4,10}$' }],
		cookies: [{ name: '_hjSessionUser_*', duration: '1 year', purpose: 'Distinguishes visitors (Hotjar)' }],
		patterns: ['_hj*'],
		render: (c) => ({
			head: `<script>(function(h,o,t,j,a,r){h.hj=h.hj||function(){(h.hj.q=h.hj.q||[]).push(arguments)};h._hjSettings={hjid:${Number(c.siteId)},hjsv:6};a=o.getElementsByTagName('head')[0];r=o.createElement('script');r.async=1;r.src=t+h._hjSettings.hjid+j+h._hjSettings.hjsv;var n=o.querySelector('script[nonce]');if(n&&n.nonce)r.nonce=n.nonce;a.appendChild(r);})(window,document,'https://static.hotjar.com/c/hotjar-','.js?sv=');</script>`
		})
	},
	meta_pixel: {
		label: 'Meta Pixel',
		category: 'marketing',
		fields: [{ name: 'pixelId', label: 'Pixel ID', placeholder: '123456789012345', pattern: '^\\d{8,20}$' }],
		cookies: [{ name: '_fbp', duration: '3 months', purpose: 'Advertising measurement (Meta)' }],
		patterns: ['_fbp', '_fbc'],
		render: (c) => ({
			head: `<script>!function(f,b,e,v,n,t,s){if(f.fbq)return;n=f.fbq=function(){n.callMethod?n.callMethod.apply(n,arguments):n.queue.push(arguments)};if(!f._fbq)f._fbq=n;n.push=n;n.loaded=!0;n.version='2.0';n.queue=[];t=b.createElement(e);t.async=!0;t.src=v;var x=b.querySelector('script[nonce]');if(x&&x.nonce)t.nonce=x.nonce;s=b.getElementsByTagName(e)[0];s.parentNode.insertBefore(t,s)}(window,document,'script','https://connect.facebook.net/en_US/fbevents.js');fbq('init',${js(c.pixelId)});fbq('track','PageView');</script>`,
			body: `<noscript><img height="1" width="1" style="display:none" alt="" src="https://www.facebook.com/tr?id=${esc(c.pixelId)}&amp;ev=PageView&amp;noscript=1"></noscript>`
		})
	},
	linkedin: {
		label: 'LinkedIn Insight Tag',
		category: 'marketing',
		fields: [{ name: 'partnerId', label: 'Partner ID', placeholder: '1234567', pattern: '^\\d{4,12}$' }],
		cookies: [{ name: 'li_fat_id, lidc, bcookie', duration: 'up to 1 year', purpose: 'Advertising measurement (LinkedIn)' }],
		patterns: ['li_*', 'lidc', 'bcookie', 'UserMatchHistory', 'AnalyticsSyncHistory'],
		render: (c) => ({
			head: `<script>window._linkedin_partner_id=${js(c.partnerId)};window._linkedin_data_partner_ids=window._linkedin_data_partner_ids||[];window._linkedin_data_partner_ids.push(window._linkedin_partner_id);(function(l){if(!l){window.lintrk=function(a,b){window.lintrk.q.push([a,b])};window.lintrk.q=[]}var s=document.getElementsByTagName("script")[0];var b=document.createElement("script");b.type="text/javascript";b.async=true;b.src="https://snap.licdn.com/li.lms-analytics/insight.min.js";var n=document.querySelector('script[nonce]');if(n&&n.nonce)b.nonce=n.nonce;s.parentNode.insertBefore(b,s);})(window.lintrk);</script>`
		})
	},
	custom: {
		label: 'Custom code',
		category: 'analytics',
		fields: [],
		cookies: [],
		patterns: [],
		render: (_c, code) => ({ head: String(code ?? '') })
	}
};

/** Validate a provider config; returns { ok, config } or { ok: false, error, field }. */
export function validateConfig(providerId, raw) {
	const p = PROVIDERS[providerId];
	if (!p) return { ok: false, error: 'Unknown provider', field: 'provider' };
	const config = {};
	for (const f of p.fields) {
		const value = String(raw?.[f.name] ?? '').trim();
		if (!value && f.optional) continue;
		if (!new RegExp(f.pattern).test(value)) return { ok: false, error: `${f.label} looks wrong (expected ${f.placeholder})`, field: `config_${f.name}` };
		config[f.name] = value;
	}
	return { ok: true, config };
}

/** "name | duration | purpose" lines → cookie objects. */
export function parseCookieLines(text) {
	return String(text ?? '')
		.split(/\r?\n/)
		.map((l) => l.trim())
		.filter(Boolean)
		.map((l) => {
			const [name, duration, purpose] = l.split('|').map((s) => s.trim());
			return { name, duration: duration ?? '', purpose: purpose ?? '' };
		});
}

export const GOOGLE_CONSENT_DEFAULT = (choice) =>
	`<script>window.dataLayer=window.dataLayer||[];function gtag(){dataLayer.push(arguments);}gtag('consent','default',${JSON.stringify(googleConsent(choice))});gtag('set','ads_data_redaction',true);gtag('set','url_passthrough',false);</script>`;

export function googleConsent(choice) {
	const g = (b) => (b ? 'granted' : 'denied');
	return {
		ad_storage: g(choice.marketing),
		ad_user_data: g(choice.marketing),
		ad_personalization: g(choice.marketing),
		analytics_storage: g(choice.analytics),
		functionality_storage: g(choice.preferences),
		personalization_storage: g(choice.preferences),
		security_storage: 'granted',
		wait_for_update: 500
	};
}
