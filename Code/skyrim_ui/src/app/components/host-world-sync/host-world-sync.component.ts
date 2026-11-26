import { ChangeDetectionStrategy, Component } from '@angular/core';
import { map } from 'rxjs/operators';
import { ClientService, HostWorldSyncStep } from '../../services/client.service';

/** The steps of joining a host world, in order, as the game reports them */
const STEPS: HostWorldSyncStep[] = ['requesting', 'received', 'saving', 'applying', 'travelling'];

/** While a joiner's world is made the host's: each step, done, under way or to come. */
@Component({
  selector: 'app-host-world-sync',
  templateUrl: './host-world-sync.component.html',
  styleUrls: ['./host-world-sync.component.scss'],
  changeDetection: ChangeDetectionStrategy.Eager,
  standalone: false,
})
export class HostWorldSyncComponent {
  public readonly sync$ = this.client.hostWorldSyncChange.pipe(
    map(sync => {
      if (!sync) {
        return null;
      }
      const finished = sync.step === 'done';
      const current = finished ? STEPS.length : STEPS.indexOf(sync.step);
      return {
        ...sync,
        finished,
        steps: STEPS.map((step, index) => ({
          key: 'COMPONENT.HOST_WORLD_SYNC.STEP_' + step.toUpperCase(),
          state: index < current ? 'done' : index === current ? 'current' : 'todo',
        })),
      };
    }),
  );

  public constructor(private readonly client: ClientService) {}
}
