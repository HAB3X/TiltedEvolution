/** Quest categories the host can share, as the server names them (Code/encoding/SharedQuests.h), in its order. */
export const QUEST_TYPES = [
  'Main',
  'MagesGuild',
  'ThievesGuild',
  'DarkBrotherhood',
  'Companions',
  'Misc',
  'Daedric',
  'Side',
  'CivilWar',
  'Dawnguard',
  'Dragonborn',
] as const;

/** Every category but Miscellaneous, which the server shares by default */
export const DEFAULT_SHARED_QUEST_TYPES: readonly string[] = QUEST_TYPES.filter(type => type !== 'Misc');

/** A saved comma separated choice, without names this version doesn't know */
export function parseSharedQuestTypes(text: string): string[] {
  return text.split(',').filter(type => (QUEST_TYPES as readonly string[]).includes(type));
}

/** The party's quest sharing as its leader set it (the partyQuestSharing event) */
export interface PartyQuestSharing {
  /** Off: no quest is shared by its kind, only those offered to the party */
  enabled: boolean;
  /** A SharedQuests mask: bit N for the server's type N, Main being 1 */
  types: number;
}

/** A running quest this player can offer the party (the shareableQuests event) */
export interface ShareableQuest {
  id: number;
  name: string;
}

/** The categories in a SharedQuests mask: QUEST_TYPES[i] is type i + 1 on the server (0 is the untyped None) */
export function questTypesFromMask(mask: number): string[] {
  return QUEST_TYPES.filter((_, index) => (mask & (1 << (index + 1))) !== 0);
}

/** The SharedQuests mask of the categories */
export function maskFromQuestTypes(types: Iterable<string>): number {
  let mask = 0;
  for (const type of types) {
    const index = (QUEST_TYPES as readonly string[]).indexOf(type);
    if (index >= 0) mask |= 1 << (index + 1);
  }
  return mask;
}
