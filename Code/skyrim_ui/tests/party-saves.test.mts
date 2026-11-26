// Co-op save names: node tests/party-saves.test.mts (pnpm test:unit)
import { parsePartySave, partySavesOf } from '../src/app/utils/party-saves.ts';
let fails = 0;
const check = (name: string, ok: boolean) => { if (!ok) { fails++; console.log('FAIL', name); } };
check('checkpoint', JSON.stringify(parsePartySave('Together_Lydia2_3', 5)) === JSON.stringify({ name: 'Together_Lydia2_3', kind: 'checkpoint', character: 'Lydia2', time: 5, slot: 3 }));
check('before host world', parsePartySave('BeforeHostWorld_Lydia', 1)?.kind === 'beforeHostWorld' && parsePartySave('BeforeHostWorld_Lydia', 1)?.character === 'Lydia');
check('before coop', parsePartySave('BeforeCoop_Lydia', 1)?.kind === 'beforeCoop');
check('the game\'s own saves', parsePartySave('Save1_4842EDB9_1_6A_Tamriel_000006_20261126042801_1_1', 1) === null && parsePartySave('Autosave2_4842EDB9_1', 1) === null);
check('checkpoint needs a slot', parsePartySave('Together_Lydia', 1) === null && parsePartySave('Together_Lydia_x', 1) === null);
check('no extra parts', parsePartySave('BeforeCoop_Lydia_2', 1) === null);
const list = [
  { name: 'Together_Lydia_1', time: 10 },
  { name: 'BeforeCoop_Lydia', time: 5 },
  { name: 'Together_Lydia_2', time: 20 },
  { name: 'Together_Serana_1', time: 30 },
  { name: 'Quicksave0', time: 40 },
];
check('one character, newest first', partySavesOf(list, 'Lydia').map(s => s.name).join() === 'Together_Lydia_2,Together_Lydia_1,BeforeCoop_Lydia');
check('everyone without a character', partySavesOf(list, '').map(s => s.name).join() === 'Together_Serana_1,Together_Lydia_2,Together_Lydia_1,BeforeCoop_Lydia');
console.log(fails ? `${fails} failed` : 'all passed'); process.exit(fails ? 1 : 0);
