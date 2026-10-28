// The otpauth:// link as an inline SVG QR code, drawn on the server so the
// setup screen needs no script and the secret never leaves the response.
import { renderSVG } from 'uqr';

export const qrSvg = (text) => renderSVG(text, { ecc: 'M', border: 2, pixelSize: 6 });
