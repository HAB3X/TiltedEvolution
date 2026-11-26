// Plus version comparison: node tests/version.test.mts (pnpm test:unit)
import { comparePlusVersions } from '../src/app/utils/version.ts';
let fails = 0;
const check = (name: string, ok: boolean) => { if (!ok) { fails++; console.log('FAIL', name); } };
check('older plus build', comparePlusVersions('v1.8.4-plus.3', 'v1.8.4-plus.4') === -1);
check('newer plus build', comparePlusVersions('v1.8.4-plus.10', 'v1.8.4-plus.9') === 1);
check('same', comparePlusVersions('v1.8.4-plus.4', 'v1.8.4-plus.4') === 0);
check('newer base version wins', comparePlusVersions('v1.8.5-plus.1', 'v1.8.4-plus.7') === 1);
check('official v1.8.4 is no plus build', comparePlusVersions('v1.8.4', 'v1.8.4-plus.4') === null);
check('a git describe build is no plus release', comparePlusVersions('v1.8.4-plus.4-3-gabc123', 'v1.8.4-plus.4') === null);
console.log(fails ? `${fails} failed` : 'all passed'); process.exit(fails ? 1 : 0);
