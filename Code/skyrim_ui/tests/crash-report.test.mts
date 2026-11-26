// Crash report issue links: node tests/crash-report.test.mts (pnpm test:unit)
import { crashIssueUrl } from '../src/app/utils/crash-report.ts';
let fails = 0;
const check = (name: string, ok: boolean) => { if (!ok) { fails++; console.log('FAIL', name); } };
const url = crashIssueUrl({
  file: 'crash_UTC_2026-11-26_10-00-00.dmp',
  folder: 'C:\\Games\\Skyrim Special Edition',
  version: 'v1.8.4-plus.4',
  game: '1.6.1170.0',
  system: 'Windows 10.0.19045 (Wine 9.0)',
});
check('the Plus repository new issue page', url.startsWith('https://github.com/HAB3X/TiltedEvolution/issues/new?'));
const params = new URL(url).searchParams;
check('titled with the version', params.get('title') === 'Crash in Skyrim Together v1.8.4-plus.4');
const body = params.get('body') ?? '';
check('body names version, game, system and the dump to attach', ['v1.8.4-plus.4', '1.6.1170.0', 'Wine 9.0', 'crash_UTC_2026-11-26_10-00-00.dmp', 'C:\\Games\\Skyrim Special Edition'].every(part => body.includes(part)));
check('only plain characters, as the game requires', /^[\x21-\x7E]+$/.test(url) && !url.includes('"'));
console.log(fails ? `${fails} failed` : 'all passed'); process.exit(fails ? 1 : 0);
