/** Skyrim Together Plus releases: "v1.8.4-plus.4" */
const PLUS_VERSION = /^v(\d+)\.(\d+)\.(\d+)-plus\.(\d+)$/;

/** -1 when a is the older Plus release, 1 when newer, 0 when the same; null unless both are Plus releases. */
export function comparePlusVersions(a: string, b: string): -1 | 0 | 1 | null {
  const pa = PLUS_VERSION.exec(a);
  const pb = PLUS_VERSION.exec(b);
  if (!pa || !pb) {
    return null;
  }
  for (let i = 1; i <= 4; i++) {
    const diff = Number(pa[i]) - Number(pb[i]);
    if (diff !== 0) {
      return diff < 0 ? -1 : 1;
    }
  }
  return 0;
}
