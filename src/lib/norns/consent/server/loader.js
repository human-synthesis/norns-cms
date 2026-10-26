// The consent loader: a tiny inline script added to every public page.
//
//  - Scripts for categories the visitor has not allowed are rendered inert,
//    inside <template data-consent="<category>">. When consent arrives
//    (`cms:consent` event from the banner) their content is activated in
//    place; scripts are re-created so they execute, carrying the page nonce
//    (CSP 'strict-dynamic' trusts them).
//  - Embeds (.embed[data-embed-src], from Markdown ::youtube / ::embed and
//    the embed block) load when marketing is allowed, or on click.
//  - Google Consent Mode gets an `update` call.
export const LOADER = `(function(){
var nonce=(document.currentScript&&document.currentScript.nonce)||'';
var state=window.__cmsConsent||{};
function loadEmbed(el){var src=el.getAttribute('data-embed-src');if(!src)return;var f=document.createElement('iframe');f.src=src;f.title=el.getAttribute('data-embed-title')||'Embedded content';f.loading='lazy';f.allow='accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture; web-share';f.setAttribute('allowfullscreen','');f.referrerPolicy='strict-origin-when-cross-origin';el.innerHTML='';el.appendChild(f);el.classList.add('embed-loaded');el.removeAttribute('data-embed-src');}
function embeds(){if(!state.marketing)return;var els=document.querySelectorAll('.embed[data-embed-src]');for(var i=0;i<els.length;i++)loadEmbed(els[i]);}
function activate(){var ts=document.querySelectorAll('template[data-consent]');for(var i=0;i<ts.length;i++){var t=ts[i];if(!state[t.getAttribute('data-consent')])continue;var nodes=Array.prototype.slice.call(t.content.childNodes);for(var k=0;k<nodes.length;k++){var n=nodes[k];if(n.nodeName==='SCRIPT'){var s=document.createElement('script');for(var a=0;a<n.attributes.length;a++)s.setAttribute(n.attributes[a].name,n.attributes[a].value);s.text=n.text;if(nonce)s.nonce=nonce;t.parentNode.insertBefore(s,t);}else{t.parentNode.insertBefore(document.importNode(n,true),t);}}t.parentNode.removeChild(t);}}
document.addEventListener('click',function(e){var b=e.target&&e.target.closest&&e.target.closest('[data-embed-load]');if(b){var el=b.closest('.embed');if(el)loadEmbed(el);}});
window.addEventListener('cms:consent',function(e){state=e.detail||{};window.__cmsConsent=state;if(typeof window.gtag==='function'){var g=function(b){return b?'granted':'denied'};window.gtag('consent','update',{ad_storage:g(state.marketing),ad_user_data:g(state.marketing),ad_personalization:g(state.marketing),analytics_storage:g(state.analytics),functionality_storage:g(state.preferences),personalization_storage:g(state.preferences)});}activate();embeds();});
if(window.MutationObserver){new MutationObserver(function(){embeds();}).observe(document.documentElement,{childList:true,subtree:true});}
embeds();
})();`;
