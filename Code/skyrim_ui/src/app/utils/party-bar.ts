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
