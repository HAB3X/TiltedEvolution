// Join code round trips: node tests/join-code.test.mts (pnpm test:unit)
import { encodeJoinCode, decodeJoinCode } from '../src/app/utils/join-code.ts';
let fails = 0;
const check = (name: string, ok: boolean) => { if (!ok) { fails++; console.log('FAIL', name); } };
for (const [a, p] of [['203.220.170.249', 10578], ['0.0.0.0', 1], ['255.255.255.255', 65535], ['192.168.1.109', 10578]] as const) {
  const code = encodeJoinCode(a, p)!;
  const back = decodeJoinCode(code);
  check(`${a}:${p} -> ${code}`, back?.address === a && back?.port === p && /^[0-9A-Z]{5}-[0-9A-Z]{5}$/.test(code));
  check(`lowercase/spaces ${code}`, decodeJoinCode(' ' + code.toLowerCase().replace('-', ' ') + ' ')?.address === a);
}
console.log('example', encodeJoinCode('203.220.170.249', 10578));
check('O and I read as 0 and 1', JSON.stringify(decodeJoinCode('OOOOO-OOOOI')) === JSON.stringify(decodeJoinCode('00000-00001')));
check('not a code', decodeJoinCode('192.168.1.5:10578') === null && decodeJoinCode('abc') === null);
check('bad address', encodeJoinCode('1.2.3', 10) === null && encodeJoinCode('1.2.3.4', 0) === null);
check('port 0 code rejected', decodeJoinCode('00000-00000') === null);
console.log(fails ? `${fails} failed` : 'all passed'); process.exit(fails ? 1 : 0);
