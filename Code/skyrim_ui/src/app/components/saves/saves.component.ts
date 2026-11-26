import { ChangeDetectionStrategy, Component, OnInit } from '@angular/core';
import { TranslocoService } from '@jsverse/transloco';
import { map } from 'rxjs/operators';
import { ClientService } from '../../services/client.service';
import { Sound, SoundService } from '../../services/sound.service';
import { PartySave, partySavesOf } from '../../utils/party-saves';

/** The Saves page: the co-op saves of this character, newest first, to load one. */
@Component({
  selector: 'app-saves',
  templateUrl: './saves.component.html',
  styleUrls: ['./saves.component.scss'],
  changeDetection: ChangeDetectionStrategy.Eager,
  standalone: false,
})
export class SavesComponent implements OnInit {
  /** null while the game lists them */
  public readonly list$ = this.client.partySavesChange.pipe(
    map(list =>
      list && {
        error: list.error,
        folder: list.folder ?? '',
        saves: partySavesOf(list.saves ?? [], list.character ?? ''),
      },
    ),
  );

  /** The save whose Load button asks to confirm */
  public confirming: string | null = null;

  public constructor(
    private readonly client: ClientService,
    private readonly sound: SoundService,
    private readonly translocoService: TranslocoService,
  ) {}

  public ngOnInit(): void {
    this.client.listPartySaves();
  }

  public label(save: PartySave): string {
    switch (save.kind) {
      case 'checkpoint':
        return this.translocoService.translate('COMPONENT.SAVES.CHECKPOINT', {
          slot: save.slot,
        });
      case 'beforeHostWorld':
        return this.translocoService.translate(
          'COMPONENT.SAVES.BEFORE_HOST_WORLD',
        );
      case 'beforeCoop':
        return this.translocoService.translate('COMPONENT.SAVES.BEFORE_COOP');
    }
  }

  public date(save: PartySave): string {
    return new Date(save.time * 1000).toLocaleString(
      this.translocoService.getActiveLang(),
      { dateStyle: 'medium', timeStyle: 'short' },
    );
  }

  /** The first click asks, the second loads: loading replaces the game being played */
  public load(save: PartySave): void {
    if (this.confirming !== save.name) {
      this.sound.play(Sound.Focus);
      this.confirming = save.name;
      return;
    }
    this.sound.play(Sound.Ok);
    this.confirming = null;
    this.client.loadPartySave(save.name);
  }
}
