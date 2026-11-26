import {
  AfterViewInit,
  Component,
  ElementRef,
  EventEmitter,
  HostListener,
  Input,
  OnDestroy,
  OnInit,
  Output,
  ViewChild,
  ChangeDetectionStrategy,
} from '@angular/core';
import { TranslocoService } from '@jsverse/transloco';
import { firstValueFrom, Subscription } from 'rxjs';
import { View } from '../../models/view.enum';
import { ClientService, LanServer } from '../../services/client.service';
import { ErrorService } from '../../services/error.service';
import { Sound, SoundService } from '../../services/sound.service';
import { StoreService } from '../../services/store.service';
import { UiRepository } from '../../store/ui.repository';
import { decodeJoinCode } from '../../utils/join-code';
import { DEFAULT_SHARED_QUEST_TYPES, parseSharedQuestTypes, QUEST_TYPES } from '../../models/shared-quests';

@Component({
  selector: 'app-connect',
  templateUrl: './connect.component.html',
  styleUrls: ['./connect.component.scss'],
  changeDetection: ChangeDetectionStrategy.Eager,
  standalone: false,
})
export class ConnectComponent implements OnInit, OnDestroy, AfterViewInit {
  public address = '';
  public password = '';
  public savePassword = false;
  public hidePassword = true;

  public connecting = false;

  /** Showing the host form instead of the connect form. */
  public hosting = false;
  public hostPassword = '';
  public hostWorld = true;
  public requireMods = true;
  public pvp = false;
  public pauseInMenus = false;
  /** Hosting for the regular Skyrim Together: the Plus-only options are off */
  public classic = false;
  /** The quest categories the party shares, names the server knows (Gameplay:sSharedQuestTypes) */
  public sharedQuestTypes = new Set<string>(DEFAULT_SHARED_QUEST_TYPES);
  public readonly questTypes = QUEST_TYPES;
  /** A relay friends join through when the router can't be opened (address:port), and its password */
  public relay = '';
  public relayPassword = '';

  /** One click setups for the host form */
  public readonly presets = {
    STORY: { hostWorld: true, requireMods: true, pvp: false },
    FREE_ROAM: { hostWorld: false, requireMods: true, pvp: false },
    PVP: { hostWorld: false, requireMods: false, pvp: true },
  } as const;

  public applyPreset(name: keyof ConnectComponent['presets']): void {
    this.sound.play(Sound.Ok);
    Object.assign(this, this.presets[name]);
  }

  public setClassic(classic: boolean): void {
    this.sound.play(Sound.Ok);
    this.classic = classic;
  }

  /** Servers on the local network, null while searching. */
  public readonly lanServers$ = this.client.lanServersChange;

  /** The mods friends will need if the host requires theirs, null until the game lists them */
  public readonly hostMods$ = this.client.hostModsChange;

  /** The Together panel's Join or Host page */
  @Input() public set mode(mode: 'join' | 'host') {
    this.hosting = mode === 'host';
    if (this.hosting) {
      this.client.listHostMods();
    }
  }

  /** The preset the boxes match, if any, to show it as chosen */
  public get activePreset(): keyof ConnectComponent['presets'] | null {
    const match = (Object.keys(this.presets) as (keyof ConnectComponent['presets'])[]).find(name => {
      const preset = this.presets[name];
      return preset.hostWorld === this.hostWorld && preset.requireMods === this.requireMods && preset.pvp === this.pvp;
    });
    return match ?? null;
  }

  public readonly presetNames = Object.keys(this.presets) as (keyof ConnectComponent['presets'])[];

  @Output() public done = new EventEmitter<void>();

  public constructor(
    private readonly client: ClientService,
    private readonly sound: SoundService,
    private readonly errorService: ErrorService,
    private readonly storeService: StoreService,
    private readonly translocoService: TranslocoService,
    private readonly uiRepository: UiRepository,
  ) {
    this.connectionSubscription = this.client.connectionStateChange.subscribe(
      async state => {
        if (this.connecting) {
          this.connecting = false;

          if (state) {
            this.sound.play(Sound.Success);
            this.done.next();
          } else if (this.errorService.getError() === '') {
            // show connection error when there is no more specific error
            const message = await firstValueFrom(
              this.translocoService.selectTranslate<string>(
                'COMPONENT.CONNECT.ERROR.CONNECTION',
              ),
            );
            await this.errorService.setError(message);
          }
        }
      },
    );

    this.protocolMismatchSubscription =
      this.client.protocolMismatchChange.subscribe(async state => {
        if (state) {
          this.connecting = false;
          const message = await firstValueFrom(
            this.translocoService.selectTranslate<string>(
              'COMPONENT.CONNECT.ERROR.VERSION_MISMATCH',
            ),
          );
          await this.errorService.setError(message);
        }
      });

    this.address = this.storeService.get('last_connected_address', '');
    this.password = this.storeService.get('last_connected_password', '');
    this.savePassword = this.password !== '';
    this.hostPassword = this.storeService.get('host_password', '');
    const saved = this.client.savedHostOptions();
    Object.assign(this, { ...saved, sharedQuestTypes: new Set(parseSharedQuestTypes(saved.sharedQuestTypes)) });
  }

  public toggleQuestType(type: string): void {
    this.sound.play(Sound.Ok);
    if (!this.sharedQuestTypes.delete(type)) {
      this.sharedQuestTypes.add(type);
    }
  }

  public searchLan(): void {
    this.client.searchLanServers();
  }

  /** Fills in the server's address; joins at once unless it needs a password that isn't typed yet. */
  public joinLan(server: LanServer): void {
    this.address = `${server.address}:${server.port}`;
    if (server.hasPassword && this.password === '') {
      this.sound.play(Sound.Ok);
      this.passwordRef.nativeElement.focus();
      return;
    }
    void this.connect();
  }

  public ngOnInit(): void {
    // Before the first check: starting it after flips the Refresh button mid-check
    if (!this.hosting) {
      this.searchLan();
    }
  }

  public ngAfterViewInit(): void {
    if (this.hosting) {
      return;
    }
    setTimeout(() => {
      this.inputRef?.nativeElement.focus();
    }, 100);
  }

  public ngOnDestroy(): void {
    this.connectionSubscription.unsubscribe();
    this.protocolMismatchSubscription.unsubscribe();
  }

  async connect(): Promise<void> {
    // A join code stands for an address and port
    const joinCode = decodeJoinCode(this.address);
    const address = joinCode
      ? ['', joinCode.address, String(joinCode.port)]
      : this.address.trim().match(/^(.+?)(?::([0-9]+))?$/);

    if (!address) {
      this.sound.play(Sound.Fail);
      const message = await firstValueFrom(
        this.translocoService.selectTranslate(
          'COMPONENT.CONNECT.ERROR.INVALID_ADDRESS',
        ),
      );
      await this.errorService.setError(message);
      return;
    }

    this.connecting = true;

    this.storeService.set('last_connected_address', this.address);
    if (this.savePassword) {
      this.storeService.set('last_connected_password', this.password);
    } else {
      this.storeService.remove('last_connected_password');
    }

    this.sound.play(Sound.Ok);
    this.client.connect(
      address[1],
      address[2] ? Number.parseInt(address[2]) : 10578,
      this.password,
    );
  }

  public host(): void {
    this.connecting = true;
    this.sound.play(Sound.Ok);
    this.client.hostServer(this.hostPassword, {
      hostWorld: this.hostWorld,
      requireMods: this.requireMods,
      pvp: this.pvp,
      pauseInMenus: this.pauseInMenus,
      classic: this.classic,
      // In the order the server lists them, so the setting reads the same every time
      sharedQuestTypes: QUEST_TYPES.filter(type => this.sharedQuestTypes.has(type)).join(','),
      relay: this.classic ? '' : this.relay.trim(),
      relayPassword: this.relayPassword,
    });
  }

  public openServerList(): void {
    this.uiRepository.openView(View.SERVER_LIST);
  }

  @ViewChild('input')
  private inputRef!: ElementRef;

  @ViewChild('passwordInput')
  private passwordRef!: ElementRef;

  private connectionSubscription: Subscription;

  private protocolMismatchSubscription: Subscription;

  @HostListener('window:keydown.escape', ['$event'])
  // @ts-ignore
  private activate(event: KeyboardEvent): void {
    // Escape closes an error box; the panel itself is a page, nothing to close
    if (this.errorService.getError()) {
      this.errorService.removeError();
      event.stopPropagation();
      event.preventDefault();
    }
  }
}
