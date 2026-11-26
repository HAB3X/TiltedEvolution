// Quest kinds to and from the server's SharedQuests mask: node tests/shared-quests.test.mts (pnpm test:unit)
import { maskFromQuestTypes, questTypesFromMask } from '../src/app/models/shared-quests.ts';
let fails = 0;
const check = (name: string, ok: boolean) => { if (!ok) { fails++; console.log('FAIL', name); } };
// Code/encoding/SharedQuests.h: Main = 1, Misc = 6, Daedric = 7, Dragonborn = 11
check('Main is bit 1', maskFromQuestTypes(['Main']) === 1 << 1);
check('Misc is bit 6 and Dragonborn bit 11', maskFromQuestTypes(['Misc', 'Dragonborn']) === ((1 << 6) | (1 << 11)));
check('unknown names are left out', maskFromQuestTypes(['Main', 'Nope']) === 1 << 1);
check('the server default (all but Misc and None) reads back', questTypesFromMask(0b111110111110).join() === 'Main,MagesGuild,ThievesGuild,DarkBrotherhood,Companions,Daedric,Side,CivilWar,Dawnguard,Dragonborn');
check('the untyped bit 0 names nothing', questTypesFromMask(1).length === 0);
check('a round trip', questTypesFromMask(maskFromQuestTypes(['Daedric', 'Side'])).join() === 'Daedric,Side');
console.log(fails ? `${fails} failed` : 'all passed'); process.exit(fails ? 1 : 0);
