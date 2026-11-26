// Party bar alerts and directions: node tests/party-bar.test.mts (pnpm test:unit)
import { compassKey, partyAlerts } from '../src/app/utils/party-bar.ts';
let fails = 0;
const check = (name: string, ok: boolean) => { if (!ok) { fails++; console.log('FAIL', name); } };
const calm = { inCombat: false, downed: false };
check('entering combat', partyAlerts(calm, { inCombat: true, downed: false }).join() === 'combat');
check('downed', partyAlerts({ inCombat: true, downed: false }, { inCombat: true, downed: true }).join() === 'downed');
check('both at once', partyAlerts(calm, { inCombat: true, downed: true }).join() === 'combat,downed');
check('staying in combat is said once', partyAlerts({ inCombat: true, downed: false }, { inCombat: true, downed: false }).length === 0);
check('leaving combat is no alert', partyAlerts({ inCombat: true, downed: true }, calm).length === 0);
check('the first update has nothing to compare', partyAlerts(undefined, { inCombat: true, downed: true }).length === 0);
check('compass points clockwise from north', [0, 1, 2, 3, 4, 5, 6, 7].map(compassKey).join() === 'N,NE,E,SE,S,SW,W,NW');
check('an unknown point is unknown', compassKey(8) === undefined && compassKey(-1) === undefined);
console.log(fails ? `${fails} failed` : 'all passed'); process.exit(fails ? 1 : 0);
