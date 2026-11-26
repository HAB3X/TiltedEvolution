/** What the game says about the last crash (its crashDump event). */
export interface CrashDump {
  /** crash_UTC_<time>.dmp */
  file: string;
  /** The folder it's in, next to SkyrimTogether.exe */
  folder: string;
  version: string;
  /** Skyrim's version */
  game: string;
  system: string;
}

/** A new issue on the Plus repository, filled in with the crash's versions and the dump to attach. Nothing is uploaded. */
export function crashIssueUrl(dump: CrashDump): string {
  const body = [
    '**What were you doing when it crashed?**',
    '',
    '',
    `**Skyrim Together:** ${dump.version}`,
    `**Skyrim:** ${dump.game}`,
    `**System:** ${dump.system}`,
    `**Crash dump:** please attach \`${dump.file}\` from \`${dump.folder}\` (drag it into this box).`,
  ].join('\n');
  const query = new URLSearchParams({ title: `Crash in Skyrim Together ${dump.version}`, body });
  // URLSearchParams writes spaces as '+', which GitHub reads as spaces
  return `https://github.com/HAB3X/TiltedEvolution/issues/new?${query.toString()}`;
}
