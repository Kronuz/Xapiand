/**
 * New-comment notifications: a fire-and-forget webhook ping so the blog owner learns a
 * comment landed (otherwise the store just writes it to D1 and nothing says so).
 *
 * The channel is configured by each tenant (slack | discord | telegram); the standard
 * provider can be inferred from its webhook URL, with `kind` as an optional override.
 * Nothing is sent unless the webhook is set. Slack and
 * Discord take a plain JSON webhook; Telegram posts to a bot `sendMessage` URL and also
 * needs `telegramChat`. Best-effort: failures are logged, never surfaced to the
 * commenter, and the POST rides `waitUntil` so it doesn't delay the response.
 */
import type { TenantConfig } from "./tenant-config.js";
import {
  notificationMessage,
  notificationPayload,
  notificationRetryDelay,
  notificationShouldRetry,
  notifyKind,
  notifyKindFromUrl,
  type NotifyInput,
} from "./notify-core.js";

export type { NotifyInput } from "./notify-core.js";

/** The bit of the Worker ExecutionContext we use (kept structural to avoid a type import). */
interface WaitUntil {
  waitUntil(p: Promise<unknown>): void;
}

async function post(url: string, body: unknown): Promise<Response> {
  return fetch(url, {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: JSON.stringify(body),
  });
}

async function deliver(url: string, kind: string, body: unknown): Promise<void> {
  let first: Response | undefined;
  try {
    first = await post(url, body);
    if (first.ok) return;
    if (!notificationShouldRetry(first.status)) {
      console.warn(`notify(${kind}) failed:`, first.status);
      return;
    }
  } catch {
    // A network failure is transient enough to merit the same single bounded retry.
  }

  const retryAfter = first?.headers.get("retry-after") ?? null;
  await new Promise((resolve) => setTimeout(resolve, notificationRetryDelay(retryAfter)));
  try {
    const second = await post(url, body);
    if (!second.ok) console.warn(`notify(${kind}) failed after retry:`, second.status);
  } catch (e: unknown) {
    console.warn(`notify(${kind}) error after retry:`, String(e));
  }
}

export function notifyNewComment(config: TenantConfig["notifications"], ctx: WaitUntil | undefined, input: NotifyInput): void {
  const url = config.webhookUrl || "";
  if (!url) return; // notifications disabled
  const configuredKind = notifyKind(config.kind);
  const kind = configuredKind || notifyKindFromUrl(url);
  if (!kind) {
    console.warn("notify: could not infer a provider from the tenant webhook; set a valid kind override");
    return;
  }
  const body = notificationPayload(kind, { telegramChat: config.telegramChat }, notificationMessage(input));
  if (!body) return;
  const task = deliver(url, kind, body);
  if (ctx && typeof ctx.waitUntil === "function") ctx.waitUntil(task);
  else void task;
}
