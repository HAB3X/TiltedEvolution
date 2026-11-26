import { Injectable } from '@angular/core';
import { TranslocoService } from '@jsverse/transloco';
import { BehaviorSubject, firstValueFrom } from 'rxjs';
import { Sound, SoundService } from './sound.service';
import { comparePlusVersions } from '../utils/version';

export interface ErrorEvent {
  error:
    | 'wrong_version'
    | 'mods_mismatch'
    | 'client_mods_disallowed'
    | 'wrong_password'
    | 'server_full'
    | 'banned'
    | 'no_reason'
    | 'bad_uGridsToLoad'
    | 'non_default_install'
    | 'set_time_public_server';
  data?: Record<any, any>;
}

export interface WrongVersionErrorEvent extends ErrorEvent {
  error: 'wrong_version';
  data: { version: string; expectedVersion: string };
}

export interface ModsMismatchErrorEvent extends ErrorEvent {
  error: 'mods_mismatch';
  // id '0': a mod the server needs; differentVersion: the player has it, but not the host's version
  data: {
    mods: [filename: string, id: string, differentVersion?: boolean][];
  };
}

export interface ClientModsDisallowedErrorEvent extends ErrorEvent {
  error: 'client_mods_disallowed';
  data: { mods: ('SKSE' | 'MO2')[] };
}

/** A mod a refused player can go and get: one the server needs, or the host's version of one they have. */
export interface RefusedMod {
  name: string;
  differentVersion: boolean;
}

export type ErrorEvents =
  | WrongVersionErrorEvent
  | ModsMismatchErrorEvent
  | ClientModsDisallowedErrorEvent
  | ErrorEvent;

@Injectable({
  providedIn: 'root',
})
export class ErrorService {
  private error = new BehaviorSubject<string>('');
  public error$ = this.error.asObservable();

  /** With a mods mismatch error: the mods to get, for the error box's search and copy actions */
  private refusedMods = new BehaviorSubject<RefusedMod[]>([]);
  public refusedMods$ = this.refusedMods.asObservable();

  /** With a version refusal between Plus releases: the error box offers the release page */
  private plusUpdate = new BehaviorSubject<boolean>(false);
  public plusUpdate$ = this.plusUpdate.asObservable();

  constructor(
    private readonly sound: SoundService,
    private readonly translocoService: TranslocoService,
  ) {}

  getError() {
    return this.error.getValue();
  }

  async setError(error: ErrorEvents | string) {
    this.sound.play(Sound.Fail);
    let message: string;
    let refusedMods: RefusedMod[] = [];
    let plusUpdate = false;
    if (typeof error === 'string') {
      message = error;
    } else {
      let data: Record<string, any> = error.data;
      let key = 'SERVICE.ERROR.ERRORS.' + error.error.toUpperCase();
      switch (error.error) {
        case 'wrong_version': {
          // Between Plus releases: say whose is older and where to get the other
          const order = comparePlusVersions(error.data.version, error.data.expectedVersion);
          if (order !== null) {
            plusUpdate = true;
            key = order < 0 ? 'SERVICE.ERROR.ERRORS.WRONG_VERSION_PLUS_UPDATE_YOU' : 'SERVICE.ERROR.ERRORS.WRONG_VERSION_PLUS_UPDATE_HOST';
          }
          break;
        }
        case 'mods_mismatch':
          refusedMods = error.data.mods
            .filter(mod => mod[1] === '0')
            .map(mod => ({ name: mod[0], differentVersion: !!mod[2] }));
          let mods = '';
          const install = error.data.mods
            .filter(mod => mod[1] === '0' && !mod[2])
            .map(mod => mod[0]);
          if (install.length > 0) {
            mods += '\n\n<hr>\n';
            mods += await firstValueFrom(
              this.translocoService.selectTranslate(
                'SERVICE.ERROR.ERRORS.MODS_MISMATCH_INSTALL',
                { mods: `<strong>${install.join(', ')}</strong>` },
              ),
            );
          }
          const version = error.data.mods
            .filter(mod => mod[2])
            .map(mod => mod[0]);
          if (version.length > 0) {
            mods += '\n\n<hr>\n';
            mods += await firstValueFrom(
              this.translocoService.selectTranslate(
                'SERVICE.ERROR.ERRORS.MODS_MISMATCH_VERSION',
                { mods: `<strong>${version.join(', ')}</strong>` },
              ),
            );
          }
          const remove = error.data.mods
            .filter(mod => mod[1] !== '0')
            .map(mod => mod[0]);
          if (remove.length > 0) {
            mods += '\n\n<hr>\n';
            mods += await firstValueFrom(
              this.translocoService.selectTranslate(
                'SERVICE.ERROR.ERRORS.MODS_MISMATCH_REMOVE',
                { mods: `<strong>${remove.join(', ')}</strong>` },
              ),
            );
          }
          data = { mods };
          break;
        case 'client_mods_disallowed':
          data = { mods: error.data.mods.join(', ') };
          break;
      }
      message = await firstValueFrom(
        this.translocoService.selectTranslate(key, data),
      );
    }
    this.refusedMods.next(refusedMods);
    this.plusUpdate.next(plusUpdate);
    this.error.next(message);
  }

  removeError() {
    this.sound.play(Sound.Ok);
    this.refusedMods.next([]);
    this.plusUpdate.next(false);
    this.error.next('');
  }
}
