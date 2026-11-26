import {
  ChangeDetectionStrategy,
  Component,
  EventEmitter,
  HostListener,
  Output,
} from '@angular/core';
import { map } from 'rxjs/operators';
import { ClientService } from '../../services/client.service';
import { Sound, SoundService } from '../../services/sound.service';
import { encodeJoinCode } from '../../utils/join-code';

/** What to send friends to join the game this player hosts. */
@Component({
  selector: 'app-invite',
  templateUrl: './invite.component.html',
  styleUrls: ['./invite.component.scss'],
  changeDetection: ChangeDetectionStrategy.Eager,
  standalone: false,
})
export class InviteComponent {
  @Output() public done = new EventEmitter<void>();

  public readonly password = this.client.hostedPassword;

  /** The join code, null until the router has said the public address. */
  public readonly code$ = this.client.hostAddressChange.pipe(
    map(address => {
      const match = address?.match(/^(.+):(\d+)$/);
      return match ? encodeJoinCode(match[1], Number(match[2])) : null;
    }),
  );

  public copied = false;

  public constructor(
    private readonly client: ClientService,
    private readonly sound: SoundService,
  ) {}

  public copy(code: string): void {
    const text = this.password
      ? `Join my Skyrim Together game: code ${code}, password ${this.password}`
      : `Join my Skyrim Together game: code ${code}`;
    // The overlay has no clipboard API; a selected text box and the copy command work
    const box = document.createElement('textarea');
    box.value = text;
    document.body.appendChild(box);
    box.select();
    this.copied = document.execCommand('copy');
    box.remove();
    this.sound.play(this.copied ? Sound.Ok : Sound.Fail);
  }

  public close(): void {
    this.sound.play(Sound.Cancel);
    this.done.next();
  }

  @HostListener('window:keydown.escape', ['$event'])
  // @ts-ignore
  private activate(event: KeyboardEvent): void {
    this.close();
    event.stopPropagation();
    event.preventDefault();
  }
}
