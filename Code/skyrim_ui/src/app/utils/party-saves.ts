/** What a co-op save is for, from the label at the start of its name (see the client's BuildSaveName). */
export type PartySaveKind = 'checkpoint' | 'beforeHostWorld' | 'beforeCoop';

export interface PartySave {
  /** The file name without .ess: what the game's Load menu shows */
  name: string;
  kind: PartySaveKind;
  /** The character's letters and digits, as in the file name */
  character: string;
  /** Party checkpoints only */
  slot?: number;
  /** When it was saved, in seconds since 1970 */
  time: number;
}

const kinds: [RegExp, PartySaveKind][] = [
  [/^Together_([A-Za-z0-9]*)_(\d+)$/, 'checkpoint'],
  [/^BeforeHostWorld_([A-Za-z0-9]*)$/, 'beforeHostWorld'],
  [/^BeforeCoop_([A-Za-z0-9]*)$/, 'beforeCoop'],
];

/** A co-op save from its file name, null for any other save. */
export function parsePartySave(name: string, time: number): PartySave | null {
  for (const [pattern, kind] of kinds) {
    const match = name.match(pattern);
    if (match) {
      const save: PartySave = { name, kind, character: match[1], time };
      if (kind === 'checkpoint') {
        save.slot = Number(match[2]);
      }
      return save;
    }
  }
  return null;
}

/** The character's co-op saves (everyone's when no character is given), newest first. */
export function partySavesOf(
  saves: { name: string; time: number }[],
  character: string,
): PartySave[] {
  return saves
    .map(save => parsePartySave(save.name, save.time))
    .filter(
      (save): save is PartySave =>
        save !== null && (character === '' || save.character === character),
    )
    .sort((a, b) => b.time - a.time);
}
