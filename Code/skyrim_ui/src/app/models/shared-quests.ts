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
