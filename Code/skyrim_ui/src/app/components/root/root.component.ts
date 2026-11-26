import { Overlay } from '@angular/cdk/overlay';
import {
  Component,
  OnInit,
  ViewChild,
  ChangeDetectionStrategy,
} from '@angular/core';
import { TranslocoService } from '@jsverse/transloco';
import { combineLatest, takeUntil } from 'rxjs';
import { faBug, faRotateRight } from '@fortawesome/free-solid-svg-icons';
import { environment } from '../../../environments/environment';
import { fadeInOutActiveAnimation } from '../../animations/fade-in-out-active.animation';
import { View } from '../../models/view.enum';
import { ClientService } from '../../services/client.service';
import { DestroyService } from '../../services/destroy.service';
import {
  SettingService,
  fontSizeToPixels,
} from '../../services/setting.service';
import { Sound, SoundService } from '../../services/sound.service';
import { UiRepository } from '../../store/ui.repository';
import { ChatComponent } from '../chat/chat.component';
import { GroupComponent } from '../group/group.component';
import { controlsAnimation } from './controls.animation';
import { notificationsAnimation } from './notifications.animation';
import { delay, filter, map, take } from 'rxjs/operators';
import { PopupNotificationService } from '../../services/popup-notification.service';

const REVEAL_EFFECT_DURATION_MS = 10000; // todo: pass value from C++?

@Component({
  selector: 'app-root',
  templateUrl: './root.component.html',
  styleUrls: ['./root.component.scss'],
  animations: [
    controlsAnimation,
    fadeInOutActiveAnimation,
    notificationsAnimation,
  ],
  host: { 'data-app-root-game': environment.game.toString() },
  providers: [DestroyService],
  changeDetection: ChangeDetectionStrategy.Eager,
  standalone: false,
})
export class RootComponent implements OnInit {
  /* ### ENUMS ### */
  readonly RootView = View;

  view$ = this.uiRepository.view$;

  connected$ = this.client.connectionStateChange.asObservable();
  menuOpen$ = this.client.openingMenuChange.asObservable();
  inGame$ = this.client.inGameStateChange.asObservable();
  active$ = this.client.activationStateChange.asObservable();
  hosting$ = this.client.hostingChange.asObservable();
  lastSession$ = this.client.lastSessionChange.asObservable();
  reconnecting$ = this.client.reconnectingChange.asObservable();
  hostWorldOffer$ = this.client.hostWorldOfferChange.asObservable();
  debugData$ = this.client.debugDataChange.asObservable();
  connectionInProgress$ =
    this.client.isConnectionInProgressChange.asObservable();
  revealingInProgress$ = false;

  @ViewChild('chat') private chatComp!: ChatComponent;
  @ViewChild(GroupComponent) private groupComponent: GroupComponent;

  public constructor(
    private readonly destroy$: DestroyService,
    private readonly client: ClientService,
    private readonly sound: SoundService,
    private readonly uiRepository: UiRepository,
    private readonly translocoService: TranslocoService,
    private readonly settingService: SettingService,
    private readonly popupNotificationService: PopupNotificationService,
    public readonly overlay: Overlay, // used for mockup
  ) {
    this.translocoService.setActiveLang(
      this.settingService.settings.language.getValue(),
    );
  }

  public rejoin(): void {
    this.sound.play(Sound.Ok);
    this.client.rejoin();
  }

  public startReadyCheck(): void {
    this.sound.play(Sound.Ok);
    this.client.startReadyCheck();
  }

  /** Stops getting back to a lost server */
  public cancelReconnect(): void {
    this.client.disconnect();
  }

  /** The last game ended while playing together (a crash): offer to go back once the player is in the game */
  private offerRejoin(): void {
    combineLatest([this.client.inGameStateChange, this.client.lastSessionChange])
      .pipe(
        filter(([inGame, session]) => inGame && session !== null),
        take(1),
        // The notification area only appears with the game view, after this; and the player gets a moment in game
        delay(2000),
        takeUntil(this.destroy$),
      )
      .subscribe(([, session]) => {
        if (this.client.connectionStateChange.getValue() || this.client.isConnectionInProgressChange.getValue()) {
          return;
        }
        this.popupNotificationService.addMessage({
          messageKey: session.hosting ? 'COMPONENT.ROOT.REHOST_OFFER' : 'COMPONENT.ROOT.REJOIN_OFFER',
          messageParams: { address: `${session.host}:${session.port}` },
          icon: faRotateRight,
          duration: 60000,
          actions: [
            {
              nameKey: session.hosting ? 'COMPONENT.ROOT.REHOST' : 'COMPONENT.HUB.REJOIN',
              callback: () => this.rejoin(),
            },
          ],
        });
      });
  }

  public ngOnInit(): void {
    this.onInGameStateSubscription();
    this.onActivationStateSubscription();
    this.onFontSizeSubscription();
    this.offerRejoin();
    this.offerCrashReport();
  }

  /** After a crash: once per crash dump, whether to report it */
  private offerCrashReport(): void {
    this.client.crashDumpChange
      .pipe(
        filter(dump => dump !== null),
        // Like the rejoin offer: the notification area appears with the game view
        delay(2000),
        takeUntil(this.destroy$),
      )
      .subscribe(() =>
        this.popupNotificationService.addMessage({
          messageKey: 'COMPONENT.ROOT.CRASH_REPORT_OFFER',
          icon: faBug,
          duration: 60000,
          actions: [
            { nameKey: 'COMPONENT.ROOT.CRASH_REPORT', callback: () => this.client.answerCrashReport(true) },
            { nameKey: 'COMPONENT.ROOT.CRASH_REPORT_NO', callback: () => this.client.answerCrashReport(false) },
          ],
        }),
      );
  }

  public onInGameStateSubscription() {
    this.client.inGameStateChange
      .pipe(takeUntil(this.destroy$))
      .subscribe(state => {
        if (!state) {
          this.closeView();
        }
      });
  }

  public onActivationStateSubscription() {
    this.client.activationStateChange
      .pipe(takeUntil(this.destroy$))
      .subscribe(state => {
        if (
          this.client.inGameStateChange.getValue() &&
          state &&
          !this.uiRepository.isViewOpen()
        ) {
          setTimeout(() => this.chatComp.focus(), 100);
        }
        if (!state) {
          this.closeView();
        }
      });
  }

  public onFontSizeSubscription() {
    this.settingService.settings.fontSize
      .pipe(
        takeUntil(this.destroy$),
        map(size => fontSizeToPixels[size]),
      )
      .subscribe(size => {
        document.documentElement.setAttribute('style', `font-size: ${size}px;`);
      });
  }

  /**
   * The page the Together panel shows: the one chosen, while it makes sense, otherwise Join when offline and
   * Players when connected. Pop-up views (disconnect, server list) keep the default page behind them.
   */
  public currentPage(view: View | null, connected: boolean): View {
    const offlineOnly = [View.CONNECT, View.HOST];
    const onlineOnly = [View.PLAYER_MANAGER, View.INVITE];
    const pages = [...offlineOnly, ...onlineOnly, View.SAVES, View.SETTINGS];
    if (
      view !== null &&
      pages.includes(view) &&
      !(connected && offlineOnly.includes(view)) &&
      !(!connected && onlineOnly.includes(view))
    ) {
      return view;
    }
    return connected ? View.PLAYER_MANAGER : View.CONNECT;
  }

  public setView(view: View | null) {
    this.uiRepository.openView(view);

    if (view) {
      this.sound.play(Sound.Focus);
    } else if (this.chatComp) {
      this.chatComp.focus();
    }
  }

  public closeView() {
    this.uiRepository.openView(null);
  }

  public reconnect(): void {
    this.client.reconnect();
  }

  public revealPlayers(): void {
    if (this.revealingInProgress$) return;

    this.revealingInProgress$ = true;
    setTimeout(() => {
      this.revealingInProgress$ = false;
    }, REVEAL_EFFECT_DURATION_MS);

    this.sound.play(Sound.Focus);
    this.client.revealPlayers();
  }
}
