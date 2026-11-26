/** What the party bar knows of a member's state, for its alerts. */
export interface MemberState {
  inCombat: boolean;
  downed: boolean;
}

export type PartyAlert = 'combat' | 'downed';

/** The alerts a member's new state raises: entering combat and going down, each once when it starts. */
export function partyAlerts(
  previous: MemberState | undefined,
  next: MemberState,
): PartyAlert[] {
  // Nothing to compare the first state with: joining a party mid-fight isn't news
  if (!previous) {
    return [];
  }
  const alerts: PartyAlert[] = [];
  if (next.inCombat && !previous.inCombat) {
    alerts.push('combat');
  }
  if (next.downed && !previous.downed) {
    alerts.push('downed');
  }
  return alerts;
}

/** The client's compass points (PartyCompass::Point), clockwise from north. */
const compassKeys = ['N', 'NE', 'E', 'SE', 'S', 'SW', 'W', 'NW'];

/** The translation key suffix of a compass point, undefined for a number that isn't one. */
export function compassKey(point: number): string | undefined {
  return compassKeys[point];
}

/** A party ping as the client reports it (partyPing). */
export interface Ping {
  /** Who pinged; empty for the local player. */
  player: string;
  /** What was pinged; empty for the spot the pinger stood on. */
  target: string;
  /** Metres from the local player, -1 when the ping is in another worldspace or cell. */
  meters: number;
  direction: number;
  place: string;
}

/** The ping's chat line (SERVICE.PARTY_PING.<line>), and the key naming the pinged spot when nothing was targeted. */
export function pingKeys(ping: Ping): { line: string; spot?: string } {
  const line = !ping.player ? 'SELF' : ping.meters >= 0 ? 'NEAR' : 'FAR';
  if (ping.target) {
    return { line };
  }
  return { line, spot: ping.player ? 'THEIR_SPOT' : 'YOUR_SPOT' };
}

/** The client's PartyTeleport::Refusal values, in order. */
const teleportRefusalKeys = [
  // kAllowed is never sent
  'UNKNOWN',
  'NOT_IN_PARTY',
  'IN_COMBAT',
  'COOLDOWN',
  'DECLINED',
  'UNAVAILABLE',
];

/** The chat line (SERVICE.PARTY_TELEPORT.<key>) for a refused go-to or summons; UNKNOWN for a reason this UI doesn't know. */
export function teleportRefusalKey(reason: number): string {
  return teleportRefusalKeys[reason] ?? 'UNKNOWN';
}
