import { ChangeDetectionStrategy, Component } from '@angular/core';
import { ClientService } from '../../services/client.service';
import { Sound, SoundService } from '../../services/sound.service';

/** Joining a host world server: what would change, and the joiner's yes or no before anything does. */
@Component({
  selector: 'app-host-world-offer',
  templateUrl: './host-world-offer.component.html',
  changeDetection: ChangeDetectionStrategy.Eager,
  standalone: false,
})
export class HostWorldOfferComponent {
  public readonly offer$ = this.client.hostWorldOfferChange;

  public constructor(
    private readonly client: ClientService,
    private readonly sound: SoundService,
  ) {}

  public answer(join: boolean): void {
    this.sound.play(join ? Sound.Ok : Sound.Cancel);
    this.client.answerHostWorld(join);
  }
}
