// Builds the JSON-LD tag outside any .n file: a literal closing script tag
// inside a component would end its <script> block. `json` is already
// escaped by seo/server/xml.js ldJson().
export const ldTag = (json) => (json ? `<script type="application/ld+json">${json}<\/script>` : '');
