/**
 * A tiny sliding-window rate limiter.
 *
 * IMPORTANT caveat vs the Python original: that backend was a single process, so an
 * in-memory limiter was authoritative. A Worker runs as many short-lived isolates, so this
 * module-level map is only *per-isolate, best-effort*. It dampens a flood but isn't a hard
 * global limit. The real distributed answer on Cloudflare is the Rate Limiting binding or a
 * Durable Object; wire one of those before relying on this in production.
 */
export class RateLimit {
  private hits = new Map<string, number[]>();
  private sinceSweep = 0;

  constructor(
    private max: number,
    private per: number,
  ) {}

  /** Record a hit for `key`; return null if allowed, or a Retry-After (seconds) if over. */
  check(key: string): number | null {
    const now = Date.now() / 1000;
    const cutoff = now - this.per;
    const q = this.hits.get(key) ?? [];
    while (q.length && q[0] <= cutoff) q.shift();
    if (q.length >= this.max) {
      return Math.max(1, Math.floor(q[0] + this.per - now) + 1);
    }
    q.push(now);
    this.hits.set(key, q);
    this.maybeSweep(now);
    return null;
  }

  private maybeSweep(now: number): void {
    if (++this.sinceSweep < 500) return;
    this.sinceSweep = 0;
    const cutoff = now - this.per;
    for (const [k, q] of this.hits) {
      while (q.length && q[0] <= cutoff) q.shift();
      if (!q.length) this.hits.delete(k);
    }
  }
}
