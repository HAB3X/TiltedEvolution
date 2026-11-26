/** How long the overlay keeps trying to get back to a server after the connection was lost. */
export const RECONNECT_FOR_MS = 120_000;

const FIRST_DELAY_MS = 2_000;
const MAX_DELAY_MS = 30_000;

/**
 * The wait before reconnect attempt `attempt` (1 for the first), doubling up to 30 s, or null when that try would
 * start more than RECONNECT_FOR_MS after the connection was lost (`elapsedMs` ago).
 */
export function reconnectDelay(attempt: number, elapsedMs: number): number | null {
  const delay = Math.min(FIRST_DELAY_MS * 2 ** (attempt - 1), MAX_DELAY_MS);
  return elapsedMs + delay > RECONNECT_FOR_MS ? null : delay;
}
