import {
  Component,
  SecurityContext,
  ChangeDetectionStrategy,
} from '@angular/core';
import { DomSanitizer, SafeHtml } from '@angular/platform-browser';
import { Observable } from 'rxjs';
import { map } from 'rxjs/operators';
import { ClientService } from '../../services/client.service';
import { ErrorService, RefusedMod } from '../../services/error.service';
import { Sound, SoundService } from '../../services/sound.service';
import { TranslocoService } from '@jsverse/transloco';
import { copyText } from '../../utils/clipboard';

@Component({
  selector: 'app-error',
  templateUrl: './error.component.html',
  styleUrls: ['./error.component.scss'],
  changeDetection: ChangeDetectionStrategy.Eager,
  standalone: false,
})
export class ErrorComponent {
  error$: Observable<SafeHtml>;
  refusedMods$ = this.errorService.refusedMods$;
  plusUpdate$ = this.errorService.plusUpdate$;
  copied = false;

  constructor(
    private readonly errorService: ErrorService,
    private readonly domSanitizer: DomSanitizer,
    private readonly client: ClientService,
    private readonly sound: SoundService,
    private readonly translocoService: TranslocoService,
  ) {
    this.error$ = this.errorService.error$.pipe(
      map(error => this.domSanitizer.sanitize(SecurityContext.HTML, error)),
      map(error => error.replace(/&#10;/g, '<br>')),
      map(error =>
        error !== '' ? this.domSanitizer.bypassSecurityTrustHtml(error) : null,
      ),
    );
  }

  removeError() {
    this.copied = false;
    this.errorService.removeError();
  }

  openReleasePage() {
    this.client.openReleasePage();
  }

  searchNexus(mod: RefusedMod) {
    this.sound.play(Sound.Ok);
    this.client.searchNexus(mod.name);
  }

  /** One mod per line, to paste to the host or into a mod manager's search */
  copyList(mods: RefusedMod[]) {
    const differentVersion = this.translocoService.translate(
      'COMPONENT.ERROR.DIFFERENT_VERSION',
    );
    const text = mods
      .map(mod =>
        mod.differentVersion ? `${mod.name} (${differentVersion})` : mod.name,
      )
      .join('\n');
    this.copied = copyText(text);
    this.sound.play(this.copied ? Sound.Ok : Sound.Fail);
  }
}
