import { Injectable, NgZone, OnDestroy } from '@angular/core';
import { SettingService } from './setting.service';
import { Ping } from '../utils/party-bar';
import { TranslocoService } from '@jsverse/transloco';
import { AsyncSubject, BehaviorSubject, ReplaySubject, Subject } from 'rxjs';
import { environment } from '../../environments/environment';
import { Debug } from '../models/debug';
import { PartyInfo } from '../models/party-info';
import { Player } from '../models/player';
import { ChatService } from './chat.service';
import { StoreService } from './store.service';
import { ErrorEvents, ErrorService } from './error.service';
import { LoadingService } from './loading.service';
import { DEFAULT_SHARED_QUEST_TYPES } from '../models/shared-quests';

/** Client game service. */
/** What joining a host's world would change. */
export interface HostWorldOffer {
  host: string;
  quests: number;
  places: number;
  people: number;
}

/** Where the player last played, kept while connected (see ClientService.rejoin). */
export interface LastSession {
  host: string;
  port: number;
  hosting: boolean;
}

const LAST_SESSION_KEY = 'last_session';

/** What the host chose on the Host page, besides the password */
export interface HostOptions {
  /** Friends who join take over the host's world */
  hostWorld: boolean;
  /** Friends need the host's mods to join */
  requireMods: boolean;
  /** Players can hurt each other */
  pvp: boolean;
  /** The world pauses while every player is in a menu */
  pauseInMenus: boolean;
  /** An official-protocol server that the regular Skyrim Together can join; the Plus-only options are off */
  classic: boolean;
  /** Comma separated quest categories the party shares (Gameplay:sSharedQuestTypes) */
  sharedQuestTypes: string;
}

/** A server on the local network, as it answered a search. */
/** The partySaves event: the save folder's co-op saves and whose they are, or why they couldn't be read */
export interface PartySavesList {
  character?: string;
  folder?: string;
  saves?: { name: string; time: number }[];
  error?: string;
}

export interface LanServer {
  address: string;
  port: number;
  name: string;
  players: number;
  maxPlayers: number;
  hasPassword: boolean;
  hostWorld: boolean;
}

@Injectable({
  providedIn: 'root',
})
export class ClientService implements OnDestroy {
  /** Initialization done. */
  public initDone = new AsyncSubject<undefined>();

  /** Activation state change. */
  public activationStateChange = new BehaviorSubject(false);

  /** InGame state change. */
  public inGameStateChange = new BehaviorSubject(!environment.game);

  /** Opening/close menu change. */
  public openingMenuChange = new BehaviorSubject(false);

  /** Connection state change. */
  public connectionStateChange = new BehaviorSubject(false);

  public isConnectionInProgressChange = new BehaviorSubject(false);

  /** Player name change. */
  public nameChange = new BehaviorSubject(environment.game ? '' : 'test');

  /** Client version setting. */
  public versionSet = new BehaviorSubject(environment.game ? '' : 'browser');

  /** Debug state change. */
  public debugStateChange = new Subject<boolean>();

  /** Debug data change. */
  public debugDataChange = new BehaviorSubject(new Debug());

  /** Connect player to server change. */
  public playerConnectedChange = new Subject<Player>();

  /** Connect party info change. */
  public partyInfoChange = new Subject<PartyInfo>();

  /** Connect party info change. */
  public partyLeftChange = new Subject<void>();

  /** Connect party invite received. */
  public partyInviteReceivedChange = new Subject<number>();

  /** Local quest started or updated while not in a party. */
  public questUpdatedChange = new Subject<void>();

  /** The party leader fast travelled, see answerPartyTravel. */
  public partyTravelChange = new Subject<{ leader: string; place: string }>();

  /** A party member went through a load door, see answerPartyDoor. */
  public partyDoorChange = new Subject<{ member: string; place: string }>();

  /** A party member, or this player, pinged a spot or a target. */
  public partyPingChange = new Subject<Ping>();

  /** Shared quests differ from another party member's (the leader's, or a member's for the leader). */
  public questDesyncChange = new Subject<{ otherPlayer: string; otherIsLeader: boolean; quests: string; more: number }>();

  /** Disconnect player to server change. */
  public playerDisconnectedChange = new Subject<Player>();

  /** Player health change. */
  public healthChange = new Subject<Player>();

  /** A party member's magicka, stamina, combat and downed state. */
  public vitalsChange = new Subject<{ playerId: number; magicka: number; stamina: number; inCombat: boolean; downed: boolean }>();

  /** How far a loaded party member is, and which way. */
  public distanceChange = new Subject<{ playerId: number; meters: number; direction: number }>();

  /** Player level change. */
  public levelChange = new Subject<Player>();

  /** Player cell change. */
  public cellChange = new Subject<Player>();

  /** Player isLoaded change. */
  public isLoadedChange = new Subject<Player>();

  public protocolMismatchChange = new BehaviorSubject(false);

  /** Receive error from core */
  public triggerError = new ReplaySubject<ErrorEvents>(1);

  /** A host world server's offer, waiting for the joiner's answer; null when there's none. */
  public hostWorldOfferChange = new BehaviorSubject<HostWorldOffer | null>(null);

  /** Whether to travel with the party leader, after partyTravelChange. */
  public answerPartyTravel(travel: boolean): void {
    skyrimtogether.answerPartyTravel(travel);
  }

  /** Whether to follow a party member through their load door, after partyDoorChange. */
  public answerPartyDoor(follow: boolean): void {
    skyrimtogether.answerPartyDoor(follow);
  }

  /** The joiner's answer to the host world offer. */
  public answerHostWorld(join: boolean): void {
    this.hostWorldOfferChange.next(null);
    skyrimtogether.answerHostWorld(join);
  }

  /** The public address:port of the game this player hosts, once the router has said it. */
  public hostAddressChange = new BehaviorSubject<string | null>(null);

  /** The co-op saves in the game's save folder, null until listed (see listPartySaves). */
  public partySavesChange = new BehaviorSubject<PartySavesList | null>(null);

  /** The mods friends need when this player hosts and requires them, null until listed (see listHostMods). */
  public hostModsChange = new BehaviorSubject<string[] | null>(null);

  /** The session the game crashed or quit in, which Rejoin returns to; null when there's none. */
  public lastSessionChange = new BehaviorSubject<LastSession | null>(null);

  /** Whether this player hosts the game they're connected to (Host a game). */
  public hostingChange = new BehaviorSubject(false);

  /** The password of the game this player hosts. */
  public get hostedPassword(): string {
    return this.hostingChange.getValue() ? this._password : '';
  }

  /** Servers found on the local network by the last search, null while searching. */
  public lanServersChange = new BehaviorSubject<LanServer[] | null>([]);

  /** Used purely for debugging. */
  public debugChange = new Subject<void>();

  // The below emitters are used in the mocking service

  /** Used for when a party leader changed. */
  public partyLaunchedChange = new Subject<void>();

  /** Used for when a party invite is sent. */
  public partyInviteChange = new Subject<number>();

  /** Used for when a party invite was accepted by the local player. */
  public partyJoinedChange = new Subject<number>();

  /** Used for when a party member was kicked. */
  public memberKickedChange = new Subject<number>();

  /** Used for when a party leader changed. */
  public partyLeaderChange = new Subject<number>();

  public localPlayerId = undefined;

  private _host: string;

  private _port: number;

  private _password: string;

  private _remainingReconnectionAttempt = environment.nbReconnectionAttempts;

  /**
   * Instantiate.
   */
  public constructor(
    private readonly zone: NgZone,
    private readonly errorService: ErrorService,
    private readonly loadingService: LoadingService,
    private readonly translocoService: TranslocoService,
    private readonly chatService: ChatService,
    private readonly settingService: SettingService,
    private readonly storeService: StoreService,
  ) {
    // The game starts with every element shown; once it's ready, it gets the player's choices and their changes, so
    // a hidden element also stops its work there
    this.initDone.subscribe(() => {
      this.settingService.shown('nameTags').subscribe(shown => skyrimtogether.setNameTags(shown));
      this.settingService.shown('pingMarkers').subscribe(shown => skyrimtogether.setPingMarkers(shown));
      this.settingService.shown('isPartyShown').subscribe(shown => skyrimtogether.setPartyBar(shown));
    });
    skyrimtogether.on('init', this.onInit.bind(this));
    skyrimtogether.on('activate', this.onActivate.bind(this));
    skyrimtogether.on('deactivate', this.onDeactivate.bind(this));
    skyrimtogether.on('enterGame', this.onEnterGame.bind(this));
    skyrimtogether.on('exitGame', this.onExitGame.bind(this));
    skyrimtogether.on('openingMenu', this.onOpeningMenu.bind(this));
    skyrimtogether.on('connect', this.onConnect.bind(this));
    skyrimtogether.on('disconnect', this.onDisconnect.bind(this));
    skyrimtogether.on('setName', this.onSetName.bind(this)); //not wanted, we dont sync name changes
    skyrimtogether.on('setVersion', this.onSetVersion.bind(this));
    skyrimtogether.on('debug', this.onDebug.bind(this)); //not needed anymore
    skyrimtogether.on('debugData', this.onUpdateDebug.bind(this));
    skyrimtogether.on('playerConnected', this.onPlayerConnected.bind(this));
    skyrimtogether.on(
      'playerDisconnected',
      this.onPlayerDisconnected.bind(this),
    );
    skyrimtogether.on('setHealth', this.onSetHealth.bind(this));
    skyrimtogether.on('setVitals', this.onSetVitals.bind(this));
    skyrimtogether.on('setPlayerDistance', this.onSetPlayerDistance.bind(this));
    skyrimtogether.on('setLevel', this.onSetLevel.bind(this));
    skyrimtogether.on('setCell', this.onSetCell.bind(this));
    skyrimtogether.on('setPlayer3dLoaded', this.onSetPlayer3dLoaded.bind(this));
    skyrimtogether.on(
      'setPlayer3dUnloaded',
      this.onSetPlayer3dUnloaded.bind(this),
    );
    skyrimtogether.on('setLocalPlayerId', this.onSetLocalPlayerId.bind(this));
    skyrimtogether.on('protocolMismatch', this.onProtocolMismatch.bind(this));
    skyrimtogether.on('triggerError', this.onTriggerError.bind(this));
    skyrimtogether.on('lanServers', this.onLanServers.bind(this));
    skyrimtogether.on('hostAddress', this.onHostAddress.bind(this));
    skyrimtogether.on('hostMods', this.onHostMods.bind(this));
    skyrimtogether.on('partySaves', this.onPartySaves.bind(this));
    skyrimtogether.on('hostWorldOffer', this.onHostWorldOffer.bind(this));

    try {
      this.lastSessionChange.next(
        JSON.parse(this.storeService.get(LAST_SESSION_KEY, 'null')),
      );
    } catch {
      this.storeService.remove(LAST_SESSION_KEY);
    }
    skyrimtogether.on('dummyData', this.onDummyData.bind(this));
    skyrimtogether.on('partyInfo', this.onPartyInfo.bind(this));
    skyrimtogether.on('partyCreated', this.onPartyCreated.bind(this));
    skyrimtogether.on('partyLeft', this.onPartyLeft.bind(this));
    skyrimtogether.on(
      'partyInviteReceived',
      this.onPartyInviteReceived.bind(this),
    );
    skyrimtogether.on('questUpdated', this.onQuestUpdated.bind(this));
    skyrimtogether.on('partyTravel', this.onPartyTravel.bind(this));
    skyrimtogether.on('partyDoor', this.onPartyDoor.bind(this));
    skyrimtogether.on('partyPing', this.onPartyPing.bind(this));
    skyrimtogether.on('questDesync', this.onQuestDesync.bind(this));
  }

  /**
   * Dispose.
   */
  public ngOnDestroy(): void {
    skyrimtogether.off('init');
    skyrimtogether.off('activate');
    skyrimtogether.off('deactivate');
    skyrimtogether.off('enterGame');
    skyrimtogether.off('exitGame');
    skyrimtogether.off('openingMenu');
    skyrimtogether.off('connect');
    skyrimtogether.off('disconnect');
    skyrimtogether.off('setName');
    skyrimtogether.off('setVersion');
    skyrimtogether.off('debug');
    skyrimtogether.off('debugData');
    skyrimtogether.off('playerConnected');
    skyrimtogether.off('playerDisconnected');
    skyrimtogether.off('setHealth');
    skyrimtogether.off('setVitals');
    skyrimtogether.off('setPlayerDistance');
    skyrimtogether.off('setLevel');
    skyrimtogether.off('setCell');
    skyrimtogether.off('setPlayer3dLoaded');
    skyrimtogether.off('setPlayer3dUnloaded');
    skyrimtogether.off('setLocalPlayerId');
    skyrimtogether.off('protocolMismatch');
    skyrimtogether.off('triggerError');
    skyrimtogether.off('lanServers');
    skyrimtogether.off('hostAddress');
    skyrimtogether.off('hostMods');
    skyrimtogether.off('partySaves');
    skyrimtogether.off('hostWorldOffer');
    skyrimtogether.off('dummyData');
    skyrimtogether.off('partyInfo');
    skyrimtogether.off('partyCreated');
    skyrimtogether.off('partyLeft');
    skyrimtogether.off('partyInviteReceived');
    skyrimtogether.off('questUpdated');
    skyrimtogether.off('partyTravel');
    skyrimtogether.off('partyDoor');
    skyrimtogether.off('partyPing');
    skyrimtogether.off('questDesync');
  }

  /**
   * Connect to a server at the given address and port.
   *
   * @param host IP address or hostname.
   * @param port Port.
   * @param password Password or admin password
   */
  /** Kick a player from the game this player hosts, or ban them. */
  public moderatePlayer(playerId: number, ban: boolean): void {
    skyrimtogether.moderatePlayer(playerId, ban);
  }

  /** Look for servers on the local network, see lanServersChange. */
  public searchLanServers(): void {
    this.lanServersChange.next(null);
    skyrimtogether.searchLanServers();
  }

  /** Returns to the last session: hosts it again, or connects to its server with the saved password. */
  public rejoin(): void {
    const session = this.lastSessionChange.getValue();
    if (!session) {
      return;
    }
    if (session.hosting) {
      this.hostServer(
        this.storeService.get('host_password', ''),
        this.savedHostOptions(),
      );
    } else {
      this.connect(
        session.host,
        session.port,
        this.storeService.get('last_connected_password', ''),
      );
    }
  }

  /** The Host page's choices from the last time this player hosted */
  public savedHostOptions(): HostOptions {
    return {
      hostWorld: this.storeService.getBool('host_world', true),
      requireMods: this.storeService.getBool('host_require_mods', true),
      pvp: this.storeService.getBool('host_pvp', false),
      pauseInMenus: this.storeService.getBool('host_pause_in_menus', false),
      classic: this.storeService.getBool('host_classic', false),
      sharedQuestTypes: this.storeService.get('host_quest_types', DEFAULT_SHARED_QUEST_TYPES.join(',')),
    };
  }

  /** Start the server next to the game and connect to it once it's up; remembers the choices for next time. */
  public hostServer(password: string, options: HostOptions): void {
    this.storeService.set('host_password', password);
    this.storeService.set('host_world', String(options.hostWorld));
    this.storeService.set('host_require_mods', String(options.requireMods));
    this.storeService.set('host_pvp', String(options.pvp));
    this.storeService.set('host_pause_in_menus', String(options.pauseInMenus));
    this.storeService.set('host_classic', String(options.classic));
    this.storeService.set('host_quest_types', options.sharedQuestTypes);
    skyrimtogether.hostServer(
      password,
      options.hostWorld,
      options.requireMods,
      options.pvp,
      options.pauseInMenus,
      options.classic,
      options.sharedQuestTypes,
    );
    this.hostingChange.next(true);
    this.isConnectionInProgressChange.next(true);
    this._host = '127.0.0.1';
    this._port = 10578;
    this._password = password;
  }

  public connect(host: string, port: number, password = ''): void {
    skyrimtogether.connect(host, port, password);
    this.hostingChange.next(false);
    this.isConnectionInProgressChange.next(true);
    this._host = host;
    this._port = port;
    this._password = password;
  }

  /**
   * Disconnect from the server or cancel connection.
   */
  public disconnect(): void {
    skyrimtogether.disconnect();
    this._remainingReconnectionAttempt = 0;
    // Left on purpose: nothing to rejoin
    this.storeService.remove(LAST_SESSION_KEY);
    this.lastSessionChange.next(null);
  }

  /**
   * Reveal players in the immediate area (lighting them up with a visual effect).
   */
  public revealPlayers(): void {
    skyrimtogether.revealPlayers();
  }

  /**
   * Launch a party.
   */
  public launchParty(): void {
    skyrimtogether.launchParty();
  }

  /**
   * Create a party invite.
   */
  public createPartyInvite(playerId: number): void {
    skyrimtogether.createPartyInvite(playerId);
  }

  /**
   * Accept a party invite.
   */
  public acceptPartyInvite(inviterId: number): void {
    skyrimtogether.acceptPartyInvite(inviterId);
  }

  /**
   * As a party leader, kick a player from the party.
   */
  public kickPartyMember(playerId: number): void {
    skyrimtogether.kickPartyMember(playerId);
  }

  /**
   * Leave a party.
   */
  public leaveParty(): void {
    skyrimtogether.leaveParty();
  }

  /**
   * As a party leader, make someone else the leader.
   */
  public changePartyLeader(playerId: number): void {
    skyrimtogether.changePartyLeader(playerId);
  }

  /**
   * Open the play guide on the wiki in the default browser.
   */
  public openPlayGuide(): void {
    skyrimtogether.openPlayGuide();
  }

  /** Search Nexus Mods for a plugin, by its file name without the extension, in the default browser. */
  public searchNexus(fileName: string): void {
    const name = fileName.replace(/\.(esp|esm|esl)$/i, '');
    skyrimtogether.openUrl(
      `https://www.nexusmods.com/skyrimspecialedition/search/?gsearch=${encodeURIComponent(name)}`,
    );
  }

  /** List the co-op saves, see partySavesChange. */
  public listPartySaves(): void {
    this.partySavesChange.next(null);
    skyrimtogether.listPartySaves();
  }

  /** Load a co-op save by its file name without .ess; the panel closes for the loading screen. */
  public loadPartySave(name: string): void {
    skyrimtogether.loadPartySave(name);
  }

  /** List the mods friends need when this player hosts and requires them, see hostModsChange. */
  public listHostMods(): void {
    skyrimtogether.listHostMods();
  }

  /**
   * Deactivate UI and release control.
   */
  public deactivate(): void {
    skyrimtogether.deactivate();
  }

  /**
   * Reconnect
   */
  public reconnect(): void {
    skyrimtogether.reconnect();
    this._remainingReconnectionAttempt = 0;
  }

  public teleportToPlayer(playerId: number): void {
    skyrimtogether.teleportToPlayer(playerId);
  }

  /**
   * Called when the UI is first initialized.
   */
  // TODO: is this still used?
  private onInit(): void {
    this.zone.run(() => {
      this.initDone.next(undefined);
      this.initDone.complete();
    });
  }

  /**
   * Called when the UI is activated.
   */
  private onActivate(): void {
    this.zone.run(() => {
      this.activationStateChange.next(true);
    });
  }

  /**
   * Called when the UI is deactivated.
   */
  private onDeactivate(): void {
    this.zone.run(() => {
      this.activationStateChange.next(false);
    });
  }

  /**
   * Called when the player enters a game.
   */
  private onEnterGame(): void {
    this.zone.run(() => {
      this.inGameStateChange.next(true);
    });
  }

  /**
   * Called when the player exits a game.
   */
  private onExitGame(): void {
    this.zone.run(() => {
      this.inGameStateChange.next(false);
    });
  }

  /**
   * Called when the player open/close a game menu
   * @param openingMenu
   */
  private onOpeningMenu(openingMenu: boolean): void {
    this.zone.run(() => {
      this.openingMenuChange.next(openingMenu);
    });
  }

  /**
   * Called when a connection is made.
   */
  private onConnect(): void {
    this.zone.run(async () => {
      this._remainingReconnectionAttempt = environment.nbReconnectionAttempts;
      this.isConnectionInProgressChange.next(false);
      this.connectionStateChange.next(true);

      this.chatService.pushSystemMessage('SERVICE.CLIENT.CONNECTED');

      // Kept until the player disconnects, so that after a crash the next start offers to rejoin
      const session: LastSession = {
        host: this._host,
        port: this._port,
        hosting: this.hostingChange.getValue(),
      };
      this.storeService.set(LAST_SESSION_KEY, JSON.stringify(session));
      this.lastSessionChange.next(null);
    });
  }

  /**
   * Called when a connection is terminated.
   */
  private onDisconnect(isError: boolean): void {
    void this.zone.run(async () => {
      this.localPlayerId = undefined;
      this.connectionStateChange.next(false);
      this.isConnectionInProgressChange.next(false);

      if (isError && this._remainingReconnectionAttempt > 0) {
        this._remainingReconnectionAttempt--;
        this.chatService.pushSystemMessage('SERVICE.CLIENT.CONNECTION_LOST');
        this.connect(this._host, this._port, this._password);
      } else {
        this.chatService.pushSystemMessage('SERVICE.CLIENT.DISCONNECTED');
      }
    });
  }

  /**
   * Called when the player's name changes.
   *
   * @param name Player's name.
   */
  private onSetName(name: string): void {
    this.zone.run(() => {
      this.nameChange.next(name);
    });
  }

  /**
   * Called when the client's version is set.
   *
   * @param version Game's version.
   */
  private onSetVersion(version: string): void {
    version = environment.overwriteVersion || version;

    this.zone.run(() => {
      this.versionSet.next(version);
    });
  }

  getVersion(): string {
    return this.versionSet.value;
  }

  /**
   * Add listener to when the player's press the F3 key
   */
  private onDebug(isDebug: boolean): void {
    this.zone.run(() => {
      this.debugStateChange.next(isDebug);
    });
  }

  private onUpdateDebug(
    numPacketsSent: number,
    numPacketsReceived: number,
    RTT: number,
    packetLoss: number,
    sentBandwidth: number,
    receivedBandwidth: number,
  ): void {
    this.zone.run(() => {
      this.debugDataChange.next(
        new Debug(
          numPacketsSent,
          numPacketsReceived,
          RTT,
          packetLoss,
          sentBandwidth,
          receivedBandwidth,
        ),
      );
    });
  }

  private onPlayerConnected(
    playerId: number,
    username: string,
    level: number,
    cellName: string,
  ) {
    if (environment.game) {
      console.log(
        `%conPlayerConnected`,
        'background: #009688; color: #fff; padding: 3px; font-size: 9px;',
        ...Array.from(arguments).map(v => JSON.stringify(v)),
      );
    }
    this.zone.run(() => {
      this.playerConnectedChange.next(
        new Player({
          name: username,
          id: playerId,
          connected: true,
          level: level,
          cellName: cellName,
        }),
      );
    });
  }

  private onPlayerDisconnected(playerId: number, username: string) {
    if (environment.game) {
      console.log(
        `%conPlayerDisconnected`,
        'background: #009688; color: #fff; padding: 3px; font-size: 9px;',
        ...Array.from(arguments).map(v => JSON.stringify(v)),
      );
    }
    this.zone.run(() => {
      this.playerDisconnectedChange.next(
        new Player({
          name: username,
          id: playerId,
          connected: false,
        }),
      );
    });
  }

  private onSetHealth(playerId: number, health: number) {
    this.zone.run(() => {
      this.healthChange.next(new Player({ id: playerId, health: health }));
    });
  }

  private onSetVitals(playerId: number, magicka: number, stamina: number, inCombat: boolean, downed: boolean) {
    this.zone.run(() => {
      this.vitalsChange.next({ playerId, magicka, stamina, inCombat, downed });
    });
  }

  private onSetPlayerDistance(playerId: number, meters: number, direction: number) {
    this.zone.run(() => {
      this.distanceChange.next({ playerId, meters, direction });
    });
  }

  private onSetLevel(playerId: number, level: number) {
    if (environment.game) {
      console.log(
        `%conSetLevel`,
        'background: #009688; color: #fff; padding: 3px; font-size: 9px;',
        ...Array.from(arguments).map(v => JSON.stringify(v)),
      );
    }
    this.zone.run(() => {
      this.levelChange.next(new Player({ id: playerId, level: level }));
    });
  }

  private onSetCell(playerId: number, cellName: string) {
    if (environment.game) {
      console.log(
        `%conSetCell`,
        'background: #009688; color: #fff; padding: 3px; font-size: 9px;',
        ...Array.from(arguments).map(v => JSON.stringify(v)),
      );
    }
    this.zone.run(() => {
      this.cellChange.next(new Player({ id: playerId, cellName: cellName }));
    });
  }

  private onSetPlayer3dLoaded(playerId: number, health: number) {
    if (environment.game) {
      console.log(
        `%conSetPlayer3dLoaded`,
        'background: #009688; color: #fff; padding: 3px; font-size: 9px;',
        ...Array.from(arguments).map(v => JSON.stringify(v)),
      );
    }
    this.zone.run(() => {
      this.isLoadedChange.next(
        new Player({ id: playerId, isLoaded: true, health: health }),
      );
    });
  }

  private onSetPlayer3dUnloaded(playerId: number) {
    if (environment.game) {
      console.log(
        `%conSetPlayer3dUnloaded`,
        'background: #009688; color: #fff; padding: 3px; font-size: 9px;',
        ...Array.from(arguments).map(v => JSON.stringify(v)),
      );
    }
    this.zone.run(() => {
      this.isLoadedChange.next(new Player({ id: playerId, isLoaded: false }));
    });
  }

  private onSetLocalPlayerId(playerId: number) {
    if (environment.game) {
      console.log(
        `%conSetLocalPlayerId`,
        'background: #009688; color: #fff; padding: 3px; font-size: 9px;',
        ...Array.from(arguments).map(v => JSON.stringify(v)),
      );
    }
    this.zone.run(() => {
      this.localPlayerId = playerId;
    });
  }

  private onProtocolMismatch() {
    this.zone.run(() => {
      this.protocolMismatchChange.next(true);
    });
  }

  private onHostWorldOffer(host: string, quests: number, places: number, people: number) {
    this.zone.run(() => {
      this.hostWorldOfferChange.next({ host, quests, places, people });
    });
  }

  private onHostAddress(address: string) {
    this.zone.run(() => {
      this.hostAddressChange.next(address);
    });
  }

  private onPartySaves(json: string) {
    this.zone.run(() => {
      this.partySavesChange.next(JSON.parse(json) as PartySavesList);
    });
  }

  private onHostMods(json: string) {
    this.zone.run(() => {
      this.hostModsChange.next(JSON.parse(json) as string[]);
    });
  }

  private onLanServers(json: string) {
    this.zone.run(() => {
      this.lanServersChange.next(JSON.parse(json) as LanServer[]);
    });
  }

  private onTriggerError(rawError: string) {
    this.zone.run(() => {
      const error = JSON.parse(rawError) as ErrorEvents;
      this.triggerError.next(error);
      void this.errorService.setError(error);
    });
  }

  private onDummyData(data: Array<number>) {
    this.zone.run(() => {
      this.debugChange.next();
      /*
      for (const numb of data) {
        console.log(numb);
      }
      console.log(data);
      */
    });
  }

  public onPartyInfo(playerIds: Array<number>, leaderId: number) {
    if (environment.game) {
      console.log(
        `%conPartyInfo`,
        'background: #009688; color: #fff; padding: 3px; font-size: 9px;',
        ...Array.from(arguments).map(v => JSON.stringify(v)),
      );
    }
    this.zone.run(() => {
      this.partyInfoChange.next(
        new PartyInfo({
          playerIds: playerIds,
          leaderId: leaderId,
        }),
      );
    });
  }

  private onPartyCreated() {
    if (environment.game) {
      console.log(
        `%conPartyCreated`,
        'background: #009688; color: #fff; padding: 3px; font-size: 9px;',
        ...Array.from(arguments).map(v => JSON.stringify(v)),
      );
    }
    this.zone.run(() => {
      this.loadingService.setLoading(false);
      this.partyInfoChange.next(
        new PartyInfo({
          playerIds: [this.localPlayerId],
          leaderId: this.localPlayerId,
        }),
      );
    });
  }

  private onPartyLeft() {
    if (environment.game) {
      console.log(
        `%conPartyLeft`,
        'background: #009688; color: #fff; padding: 3px; font-size: 9px;',
        ...Array.from(arguments).map(v => JSON.stringify(v)),
      );
    }
    this.zone.run(() => {
      this.partyLeftChange.next();
    });
  }

  private onPartyInviteReceived(inviterId: number) {
    if (environment.game) {
      console.log(
        `%conPartyInviteReceived`,
        'background: #009688; color: #fff; padding: 3px; font-size: 9px;',
        ...Array.from(arguments).map(v => JSON.stringify(v)),
      );
    }
    this.zone.run(() => {
      this.partyInviteReceivedChange.next(inviterId);
    });
  }

  private onQuestUpdated() {
    this.zone.run(() => {
      this.questUpdatedChange.next();
    });
  }

  private onPartyPing(player: string, target: string, meters: number, direction: number, place: string) {
    this.zone.run(() => {
      this.partyPingChange.next({ player, target, meters, direction, place });
    });
  }

  private onPartyDoor(member: string, place: string) {
    this.zone.run(() => {
      this.partyDoorChange.next({ member, place });
    });
  }

  private onPartyTravel(leader: string, place: string) {
    this.zone.run(() => {
      this.partyTravelChange.next({ leader, place });
    });
  }

  private onQuestDesync(otherPlayer: string, otherIsLeader: boolean, quests: string, more: number) {
    this.zone.run(() => {
      this.questDesyncChange.next({ otherPlayer, otherIsLeader, quests, more });
    });
  }
}
