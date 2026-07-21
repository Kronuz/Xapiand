export const ACCESS_KEY_RE = /^[A-Za-z0-9_-]{43}$/;

export function isAccessKey(value: string): boolean {
  return value === "" || ACCESS_KEY_RE.test(value);
}

export function accessKeyMatches(expected: string, provided: string): boolean {
  if (!expected) return true;
  if (expected.length !== provided.length) return false;
  let value = 0;
  for (let i = 0; i < expected.length; i++) value |= expected.charCodeAt(i) ^ provided.charCodeAt(i);
  return value === 0;
}
