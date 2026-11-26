import { ChangeDetectionStrategy, Component } from '@angular/core';
import { combineLatest } from 'rxjs';
import { map } from 'rxjs/operators';
import { ClientService } from '../../services/client.service';

/** How the connection is doing: a coloured dot and the ping, always on screen while playing together. */
type Quality = 'good' | 'slow' | 'bad' | 'connecting';

// Round trip times above these feel laggy, then broken
const SLOW_MS = 120;
const BAD_MS = 250;

@Component({
  selector: 'app-connection-status',
  templateUrl: './connection-status.component.html',
  styleUrls: ['./connection-status.component.scss'],
  changeDetection: ChangeDetectionStrategy.Eager,
  standalone: false,
})
export class ConnectionStatusComponent {
  public readonly status$ = combineLatest([
    this.client.connectionStateChange,
    this.client.isConnectionInProgressChange,
    this.client.debugDataChange,
  ]).pipe(
    map(([connected, connecting, debug]) => {
      if (connecting) {
        return { quality: 'connecting' as Quality, ping: null };
      }
      if (!connected) {
        return null;
      }
      const ping = debug.RTT;
      const quality: Quality =
        ping > BAD_MS ? 'bad' : ping > SLOW_MS ? 'slow' : 'good';
      return { quality, ping };
    }),
  );

  public constructor(private readonly client: ClientService) {}
}
