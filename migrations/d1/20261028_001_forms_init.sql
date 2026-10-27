-- mirrored from migrations/forms/20261028_001_init.sql by tools/d1-sync.js; edit the source, not this copy
-- Forms (admin-defined fields, delivery options, translations) and their
-- submissions. Seeded with the two default forms: contact and newsletter.
CREATE TABLE IF NOT EXISTS forms (
	id           INTEGER PRIMARY KEY AUTOINCREMENT,
	slug         TEXT    NOT NULL UNIQUE,
	name         TEXT    NOT NULL,
	kind         TEXT    NOT NULL DEFAULT 'custom',  -- contact | newsletter | custom
	fields       TEXT    NOT NULL DEFAULT '[]',      -- [{ name, label, type, required, options, placeholder, help, autocomplete }]
	settings     TEXT    NOT NULL DEFAULT '{}',      -- submitLabel, successMessage, duplicateMessage, notify, notifyTo, notifySubject, dedupe, redirectTo
	translations TEXT    NOT NULL DEFAULT '{}',      -- { de: { name, submitLabel, successMessage, duplicateMessage, fields: { <name>: { label, placeholder, help, options } } } }
	enabled      INTEGER NOT NULL DEFAULT 1,
	created_at   INTEGER NOT NULL,
	updated_at   INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS form_submissions (
	id          INTEGER PRIMARY KEY AUTOINCREMENT,
	form_id     INTEGER NOT NULL REFERENCES forms(id) ON DELETE CASCADE,
	data        TEXT    NOT NULL,                    -- { <field>: value }, consent texts included as shown
	email       TEXT    NOT NULL DEFAULT '',         -- the first email field, lowercased (search, dedupe)
	status      TEXT    NOT NULL DEFAULT 'new',      -- new | read | archived | spam
	locale      TEXT    NOT NULL DEFAULT '',
	page        TEXT    NOT NULL DEFAULT '',         -- path the form was sent from
	user_id     INTEGER,
	notified    INTEGER NOT NULL DEFAULT 0,          -- 1 sent, 0 not asked, -1 failed
	notify_error TEXT   NOT NULL DEFAULT '',
	created_at  INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS form_submissions_form_idx ON form_submissions(form_id, status, created_at);
CREATE INDEX IF NOT EXISTS form_submissions_email_idx ON form_submissions(form_id, email);

INSERT OR IGNORE INTO forms (slug, name, kind, fields, settings, translations, enabled, created_at, updated_at) VALUES
	('contact', 'Contact', 'contact', '[{"name": "name", "label": "Name", "type": "text", "required": true, "autocomplete": "name"}, {"name": "email", "label": "Email", "type": "email", "required": true, "autocomplete": "email"}, {"name": "message", "label": "Message", "type": "textarea", "required": true}, {"name": "consent", "label": "I agree that my details are used to answer my message.", "type": "consent", "required": true}]', '{"submitLabel": "Send message", "successMessage": "Thanks — we received your message and will get back to you soon.", "notify": true, "notifyTo": [], "notifySubject": "New message from {name}", "dedupe": false}', '{"de": {"name": "Kontakt", "submitLabel": "Nachricht senden", "successMessage": "Danke — wir haben Ihre Nachricht erhalten und melden uns bald.", "fields": {"name": {"label": "Name"}, "email": {"label": "E-Mail"}, "message": {"label": "Nachricht"}, "consent": {"label": "Ich bin einverstanden, dass meine Angaben zur Beantwortung meiner Nachricht verwendet werden."}}}, "fr": {"name": "Contact", "submitLabel": "Envoyer le message", "successMessage": "Merci — nous avons bien reçu votre message et vous répondrons bientôt.", "fields": {"name": {"label": "Nom"}, "email": {"label": "E-mail"}, "message": {"label": "Message"}, "consent": {"label": "J’accepte que mes données soient utilisées pour répondre à mon message."}}}, "es": {"name": "Contacto", "submitLabel": "Enviar mensaje", "successMessage": "Gracias — hemos recibido tu mensaje y te responderemos pronto.", "fields": {"name": {"label": "Nombre"}, "email": {"label": "Correo electrónico"}, "message": {"label": "Mensaje"}, "consent": {"label": "Acepto que mis datos se usen para responder a mi mensaje."}}}, "it": {"name": "Contatti", "submitLabel": "Invia messaggio", "successMessage": "Grazie — abbiamo ricevuto il tuo messaggio e ti risponderemo presto.", "fields": {"name": {"label": "Nome"}, "email": {"label": "Email"}, "message": {"label": "Messaggio"}, "consent": {"label": "Accetto che i miei dati vengano usati per rispondere al mio messaggio."}}}, "ro": {"name": "Contact", "submitLabel": "Trimite mesajul", "successMessage": "Mulțumim — am primit mesajul tău și îți răspundem în curând.", "fields": {"name": {"label": "Nume"}, "email": {"label": "E-mail"}, "message": {"label": "Mesaj"}, "consent": {"label": "Sunt de acord ca datele mele să fie folosite pentru a răspunde la mesaj."}}}}', 1, 0, 0),
	('newsletter', 'Newsletter', 'newsletter', '[{"name": "email", "label": "Email", "type": "email", "required": true, "autocomplete": "email", "placeholder": "you@example.com"}, {"name": "consent", "label": "Send me the newsletter. I can unsubscribe at any time.", "type": "consent", "required": true}]', '{"submitLabel": "Subscribe", "successMessage": "You’re subscribed. Thank you!", "duplicateMessage": "You’re already subscribed.", "notify": true, "notifyTo": [], "notifySubject": "New newsletter subscriber: {email}", "dedupe": true}', '{"de": {"name": "Newsletter", "submitLabel": "Abonnieren", "successMessage": "Sie haben den Newsletter abonniert. Danke!", "duplicateMessage": "Sie haben den Newsletter bereits abonniert.", "fields": {"email": {"label": "E-Mail", "placeholder": "sie@beispiel.de"}, "consent": {"label": "Schicken Sie mir den Newsletter. Ich kann ihn jederzeit abbestellen."}}}, "fr": {"name": "Newsletter", "submitLabel": "S’abonner", "successMessage": "Vous êtes abonné·e. Merci !", "duplicateMessage": "Vous êtes déjà abonné·e.", "fields": {"email": {"label": "E-mail", "placeholder": "vous@exemple.fr"}, "consent": {"label": "Envoyez-moi la newsletter. Je peux me désabonner à tout moment."}}}, "es": {"name": "Boletín", "submitLabel": "Suscribirme", "successMessage": "Ya estás suscrito. ¡Gracias!", "duplicateMessage": "Ya estabas suscrito.", "fields": {"email": {"label": "Correo electrónico", "placeholder": "tu@ejemplo.com"}, "consent": {"label": "Envíame el boletín. Puedo darme de baja cuando quiera."}}}, "it": {"name": "Newsletter", "submitLabel": "Iscriviti", "successMessage": "Iscrizione completata. Grazie!", "duplicateMessage": "Sei già iscritto.", "fields": {"email": {"label": "Email", "placeholder": "tu@esempio.it"}, "consent": {"label": "Inviatemi la newsletter. Posso disiscrivermi in qualsiasi momento."}}}, "ro": {"name": "Newsletter", "submitLabel": "Abonează-mă", "successMessage": "Te-ai abonat. Mulțumim!", "duplicateMessage": "Ești deja abonat.", "fields": {"email": {"label": "E-mail", "placeholder": "tu@exemplu.ro"}, "consent": {"label": "Trimiteți-mi newsletterul. Mă pot dezabona oricând."}}}}', 1, 0, 0);

-- Editors and viewers read submissions; editors also manage forms.
UPDATE roles SET capabilities = json_insert(json_insert(capabilities, '$[#]', 'forms.view'), '$[#]', 'forms.manage') WHERE id = 'editor' AND capabilities NOT LIKE '%forms.%';
UPDATE roles SET capabilities = json_insert(capabilities, '$[#]', 'forms.view') WHERE id = 'viewer' AND capabilities NOT LIKE '%forms.%';
