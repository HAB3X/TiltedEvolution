// Reconnect backoff: node tests/reconnect.test.mts (pnpm test:unit)
import { reconnectDelay, RECONNECT_FOR_MS } from '../src/app/utils/reconnect.ts';
let fails = 0;
const check = (name: string, ok: boolean) => { if (!ok) { fails++; console.log('FAIL', name); } };
check('first attempt after 2 s', reconnectDelay(1, 0) === 2000);
check('doubles', reconnectDelay(2, 2000) === 4000 && reconnectDelay(3, 6000) === 8000 && reconnectDelay(4, 14000) === 16000);
check('at most 30 s apart', reconnectDelay(6, 40000) === 30000 && reconnectDelay(9, 60000) === 30000);
check('stops once the next try would be past the limit', reconnectDelay(5, RECONNECT_FOR_MS - 1000) === null);
check('a try exactly at the limit still happens', reconnectDelay(1, RECONNECT_FOR_MS - 2000) === 2000);
// A server that never answers: each failed try takes about 10 s before the next wait
let elapsed = 0, attempts = 0;
for (let delay = reconnectDelay(1, 0); delay !== null; delay = reconnectDelay(++attempts + 1, elapsed)) elapsed += delay + 10000;
check(`gives up within about two minutes (${attempts} tries, ${elapsed} ms)`, attempts >= 4 && elapsed <= RECONNECT_FOR_MS + 10000);
console.log(fails ? `${fails} failed` : 'all passed'); process.exit(fails ? 1 : 0);
