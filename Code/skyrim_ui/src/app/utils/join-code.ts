/**
 * Join codes: a server's IPv4 address and port as ten characters, like `5Z3K8-M2QRT`, easier to read out than an
 * address. Crockford base32 of the 4 address bytes and 2 port bytes, so letters that look alike (I, L, O, U) never
 * appear and are read as their look-alikes when typed.
 */
const ALPHABET = '0123456789ABCDEFGHJKMNPQRSTVWXYZ';

export function encodeJoinCode(address: string, port: number): string | null {
  const parts = address.split('.').map(Number);
  if (
    parts.length !== 4 ||
    parts.some(p => !Number.isInteger(p) || p < 0 || p > 255) ||
    !Number.isInteger(port) ||
    port < 1 ||
    port > 65535
  ) {
    return null;
  }

  let value = 0n;
  for (const byte of [...parts, port >> 8, port & 0xff]) {
    value = (value << 8n) | BigInt(byte);
  }
  let code = '';
  for (let i = 0; i < 10; i++) {
    code = ALPHABET[Number(value & 31n)] + code;
    value >>= 5n;
  }
  return `${code.slice(0, 5)}-${code.slice(5)}`;
}

/** The address and port of a join code, or null when the text isn't one. */
export function decodeJoinCode(
  text: string,
): { address: string; port: number } | null {
  const cleaned = text
    .trim()
    .toUpperCase()
    .replace(/[\s-]/g, '')
    .replace(/[IL]/g, '1')
    .replace(/O/g, '0')
    .replace(/U/g, 'V');
  if (!/^[0-9A-Z]{10}$/.test(cleaned)) {
    return null;
  }

  let value = 0n;
  for (const char of cleaned) {
    value = (value << 5n) | BigInt(ALPHABET.indexOf(char));
  }
  const bytes: number[] = [];
  for (let i = 0; i < 6; i++) {
    bytes.unshift(Number(value & 0xffn));
    value >>= 8n;
  }
  const port = (bytes[4] << 8) | bytes[5];
  if (port === 0) {
    return null;
  }
  return { address: bytes.slice(0, 4).join('.'), port };
}
