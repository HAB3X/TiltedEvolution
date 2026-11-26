import { createStore } from '@ngneat/elf';
import {
  addEntities,
  deleteEntities,
  getAllEntities,
  getEntity,
  selectAllEntities,
  updateEntities,
  withEntities,
} from '@ngneat/elf-entities';
import { EventEmitter } from 'events';
import { fromEvent } from 'rxjs';
import { MessageTypes } from '../services/chat.service';
import { ErrorEvents } from '../services/error.service';
import { MockPlayer } from './mock-player';

let nextPlayerId = 1;

const playerStore = createStore(
  { name: 'players' },
  withEntities<MockPlayer>(),
);

export class SkyrimtogetherMock extends EventEmitter implements SkyrimTogether {
  private connected = false;
  private active = false;
  private version = 'browser';
  private playerName = 'Local Player';
  private showEvents = true;
  private localPlayerId: number;
  // 't-drop': connects, loses the connection 3 s later, then fails twice before getting back
  private dropTries = 0;
  public readonly players$ = playerStore.pipe(selectAllEntities());

  answerHostWorld(join: boolean): void {
    console.log(`host world: ${join ? 'join' : 'keep'}`);
    if (!join) {
      this.emit('hostWorldSync', 'cancelled', 0, 0, 0);
      return;
    }
    this.emit('hostWorldSync', 'saving', 0, 0, 0);
    setTimeout(() => this.emit('hostWorldSync', 'applying', 0, 0, 0), 1500);
    setTimeout(() => this.emit('hostWorldSync', 'travelling', 12, 3, 2), 2000);
    setTimeout(() => this.emit('hostWorldSync', 'done', 12, 3, 2), 3000);
  }

  setPingMarkers(enabled: boolean): void {
    console.log(`ping markers: ${enabled ? 'on' : 'off'}`);
  }

  setPerfOverlay(enabled: boolean): void {
    clearInterval(this.perfTimer);
    if (enabled) {
      this.perfTimer = setInterval(() => this.emit('perfData', 58 + Math.random() * 4, 16.7, 0.4 + Math.random() * 0.3, 42, 37), 1000);
    }
  }

  private perfTimer: ReturnType<typeof setInterval> | undefined;

  setPartyBar(enabled: boolean): void {
    console.log(`party bar: ${enabled ? 'on' : 'off'}`);
  }

  setNameTags(enabled: boolean): void {
    console.log(`name tags ${enabled}`);
  }

  // A vote as the server would run it, with one other player saying yes after a second
  startVote(kind: number, targetId: number): void {
    const id = Date.now() % 1000;
    this.emit('vote', id, kind, 0, this.localPlayerId, targetId, 1, 0, kind === 0 ? 2 : 3, kind === 0 ? 60 : 30);
    setTimeout(() => this.emit('vote', id, kind, kind === 0 ? 1 : 0, this.localPlayerId, targetId, 2, 0, kind === 0 ? 2 : 3, 0), 1500);
  }

  // The newest crash dump, as the game sends it on entering a game: call from the console to try the crash report
  sendCrashDump(): void {
    this.emit('crashDump', 'crash_UTC_2026-11-26_10-00-00.dmp', 'C:\\Games\\Skyrim Special Edition', 'v1.8.4-plus.4', '1.6.1170.0', 'Windows 10.0.19045 (Wine 9.0)');
  }

  answerVote(voteId: number, yes: boolean): void {
    console.log(`vote ${voteId}: ${yes ? 'yes' : 'no'}`);
  }

  moderatePlayer(playerId: number, ban: boolean): void {
    console.log(`${ban ? 'ban' : 'kick'} ${playerId}`);
  }

  searchLanServers(): void {
    setTimeout(() => {
      this.emit(
        'lanServers',
        JSON.stringify([
          { address: '192.168.1.20', port: 10578, name: 'Mock co-op', players: 1, maxPlayers: 8, hasPassword: true, hostWorld: true },
        ]),
      );
    }, 500);
  }

  listPartySaves(): void {
    const now = Math.floor(Date.now() / 1000);
    setTimeout(
      () =>
        this.emit(
          'partySaves',
          JSON.stringify({
            character: 'Mock',
            folder: 'C:\\Users\\Mock\\Documents\\My Games\\Skyrim Special Edition\\Saves',
            saves: [
              { name: 'Together_Mock_3', time: now - 600 },
              { name: 'Together_Mock_2', time: now - 7200 },
              { name: 'BeforeHostWorld_Mock', time: now - 86400 },
              { name: 'BeforeCoop_Mock', time: now - 90000 },
              { name: 'Together_Other_1', time: now - 60 },
            ],
          }),
        ),
      300,
    );
  }

  loadPartySave(name: string): void {
    console.log(`load ${name}`);
  }

  listHostMods(): void {
    setTimeout(
      () =>
        this.emit(
          'hostMods',
          JSON.stringify(['SkyUI_SE.esp', 'Unofficial Skyrim Special Edition Patch.esp', 'RaceMenu.esp']),
        ),
      300,
    );
  }

  hostServer(
    password: string,
    hostWorld: boolean,
    requireMods: boolean,
    pvp: boolean,
    pauseInMenus: boolean,
    classic: boolean,
    sharedQuestTypes: string,
    relay: string,
    relayPassword: string,
    perPlayerLoot: boolean,
    revive: boolean,
  ): void {
    console.log(`host${relay ? ` through the relay ${relay}` : ''}`);
    this.connect('127.0.0.1', 10578, password);
    setTimeout(() => this.emit('hostAddress', '203.0.113.7:10578'), 1000);
  }

  connect(host: string, port: number, password: string, returning = false): void {
    console.log(`connect ${host}:${port}${returning ? ', returning' : ''}`);
    if (host === 't-drop' && this.dropTries++ > 0 && this.dropTries <= 3) {
      setTimeout(() => this.emit('disconnect', true), 250);
      return;
    }
    // 't-world-sync': a host world server, which asks before the joiner's world changes
    if (host === 't-world-sync') {
      setTimeout(() => this.emit('hostWorldSync', 'requesting', 0, 0, 0), 500);
      setTimeout(() => {
        this.emit('hostWorldSync', 'received', 0, 0, 0);
        this.emit('hostWorldOffer', 'MockHost', 12, 3, 2);
      }, 2000);
    }
    if (host === 't-drop' && this.dropTries === 1) {
      setTimeout(() => {
        this.connected = false;
        this.emit('disconnect', true);
      }, 3000);
    }
    if (!this.connected) {
      let error: ErrorEvents | boolean;
      switch (host) {
        case 't-port':
        case 't-host':
          error = true;
          break;
        case 't-password':
          if (password !== 'test') {
            error = { error: 'wrong_password' };
          }
          break;
        case 't-version':
          error = {
            error: 'wrong_version',
            data: {
              version: '[current-version]',
              expectedVersion: '[expected-version]',
            },
          };
          break;
        case 't-full':
          error = { error: 'server_full' };
          break;
        case 't-client-mods':
          error = { error: 'client_mods_disallowed', data: { mods: ['SKSE'] } };
          break;
        case 't-mods':
          error = {
            error: 'mods_mismatch',
            data: {
              mods: [
                ['missing.esp', '0', false],
                ['older.esp', '0', true],
                ['remove.esp', '12'],
                ['missing_2.esp', '0'],
                ['remove_2.esp', '12'],
              ],
            },
          };
          break;
      }
      setTimeout(() => {
        this.emit(!!error ? 'disconnect' : 'connect');
        this.connected = !error;
        if (error && typeof error !== 'boolean') {
          this.emit('triggerError', JSON.stringify(error));
        } else {
          this.emit('setLocalPlayerId', (this.localPlayerId = nextPlayerId++));
        }
        for (const player of playerStore.query(getAllEntities())) {
          this.emit(
            'playerConnected',
            player.id,
            player.name,
            player.level,
            player.cellName,
          );
          this.emit('setPlayer3dLoaded', player.id, player.health);
        }
      }, 250);
    }
  }

  disconnect(): void {
    if (this.connected) {
      this.emit('disconnect');
      this.connected = false;
    }
  }

  reconnect(): void {
    throw new Error('NOT YET IMPLEMENTED');
  }

  revealPlayers(): void {
    this.sendMessage(MessageTypes.SYSTEM_MESSAGE, "Revealing players...");
  }

  setTime(hours: number, minutes: number): void {
    this.sendMessage(MessageTypes.SYSTEM_MESSAGE, `Setting time to "${hours}:${minutes}"!`);
  }

  wave(): void {
    this.sendMessage(MessageTypes.SYSTEM_MESSAGE, 'You wave.');
  }

  sendMessage(type: MessageTypes, message: string): void {
    if (this.connected) {
      this.emit('message', type, message, this.playerName);
    }
  }

  deactivate(): void {
    throw new Error('NOT YET IMPLEMENTED');
  }

  teleportToPlayer(playerId: number): void {
    if (this.connected) {
      const player = playerStore.query(getEntity(playerId));
      if (player) {
        console.log(
          `%cTELEPORT`,
          'background: #F09688; color: #fff; padding: 3px; font-size: 9px;',
          'Teleport to player',
          JSON.stringify(player.name),
          `(${player.id})`,
          'in',
          JSON.stringify(player.cellName),
        );
      }
    }
  }

  launchParty(): void {
    if (this.connected) {
      this.emit('partyCreated');
      // The server's default: every kind but Misc
      this.emit('partyQuestSharing', true, 0b111110111110);
    }
  }

  createPartyInvite(playerId: number): void {
    playerStore.update(updateEntities(playerId, { invited: true }));
  }

  acceptPartyInvite(inviterId: number): void {
    this.emit('partyCreated');
    this.emit('partyQuestSharing', true, 0b111110111110);
    this.emit(
      'partyInfo',
      [
        ...playerStore
          .query(getAllEntities())
          .filter(p => p.isInGroup)
          .map(p => p.id),
      ],
      inviterId,
    );
  }

  kickPartyMember(playerId: number): void {
    playerStore.update(updateEntities(playerId, { isInGroup: false }));
    this.emit(
      'partyInfo',
      playerStore
        .query(getAllEntities())
        .filter(p => p.isInGroup)
        .map(p => p.id),
      this.localPlayerId,
    );
  }

  leaveParty(): void {
    if (this.connected) {
      this.emit('partyInfo', [], -1);
      this.emit('partyLeft');
    }
  }

  changePartyLeader(playerId: number): void {
    playerStore.update(updateEntities(playerId, { hasOwnParty: true }));
    this.emit(
      'partyInfo',
      [
        ...playerStore
          .query(getAllEntities())
          .filter(p => p.isInGroup)
          .map(p => p.id),
      ],
      playerId,
    );
  }

  initMock() {
    Object.keys((this as any)._events).forEach(e => {
      this.on(e, (...params) => {
        if (this.showEvents) {
          const eventName = e
            .replace(/(\G(?!^)|\b[a-zA-Z][a-z]*)([A-Z][a-z]*|\d+)/gm, `$1_$2`)
            .toUpperCase();
          console.log(
            `%cEVENT`,
            'background: #009688; color: #fff; padding: 3px; font-size: 9px;',
            `[${eventName}]`,
            ...params.map(v => JSON.stringify(v)),
          );
        }
      });
    });
    this.emit('init');
    this.emit('enterGame');
    this.emit('setVersion', this.version);
    this.emit('setName', this.playerName);

    fromEvent(window, 'keydown').subscribe((event: KeyboardEvent) => {
      if (event.ctrlKey && event.location === 2) {
        this.active = !this.active;
        this.emit(this.active ? 'activate' : 'deactivate');
        event.preventDefault();
        return;
      }
      switch (event.key) {
        case 'F2': {
          this.active = !this.active;
          this.emit(this.active ? 'activate' : 'deactivate');
          event.preventDefault();
          break;
        }
      }
    });
  }

  setMockVersion(version: string): void {
    this.version = version;
  }

  setMockPlayerName(name: string): void {
    this.playerName = name;
  }

  setShowEvents(show: boolean): void {
    this.showEvents = show;
  }

  addMockPlayer() {
    const newPlayerId = nextPlayerId++;
    const cities = [
      'Whiterun',
      'Dawnstar',
      'Falkreath',
      'Markarth',
      'Morthal',
      'Riften',
      'Solitude',
      'Windhelm',
    ];
    const newPlayer: MockPlayer = {
      name: 'Player ' + newPlayerId,
      id: newPlayerId,
      level: Math.floor(Math.random() * 100),
      health: Math.floor(Math.random() * 100),
      cellName: cities[Math.floor(Math.random() * cities.length)],
      hasOwnParty: false,
      isInGroup: false,
      invited: false,
      invitedLocalPlayer: false,
    };
    playerStore.update(addEntities(newPlayer));
    if (this.connected) {
      this.emit(
        'playerConnected',
        newPlayer.id,
        newPlayer.name,
        newPlayer.level,
        newPlayer.cellName,
      );
      this.emit('setPlayer3dLoaded', newPlayer.id, newPlayer.health);
    }
    // this.emit('setHealth', newPlayer.id, newPlayer.health);
    return newPlayer;
  }

  disconnectMockPlayer(playerId: number) {
    const mockPlayer = playerStore.query(getEntity(playerId));
    if (mockPlayer) {
      if (this.connected) {
        this.emit('playerDisconnected', mockPlayer.id, mockPlayer.name);
      }
      playerStore.update(deleteEntities(mockPlayer.id));
    }
  }

  accteptMockPlayerInvite(playerId: number) {
    playerStore.update(
      updateEntities(playerId, { invited: false, isInGroup: true }),
    );
    this.emit(
      'partyInfo',
      playerStore
        .query(getAllEntities())
        .filter(p => p.isInGroup)
        .map(p => p.id),
      this.localPlayerId,
    );
  }

  openUrl(url: string): void {
    window.open(url, '_blank');
  }

  openPlayGuide(): void {
    window.open(
      'https://wiki.tiltedphoques.com/tilted-online/general-information/playguide',
      '_blank',
    );
  }

  mockQuestUpdate() {
    this.emit('questUpdated');
  }

  /** Every player's party bar: random magicka and stamina, distance and direction, and combat or downed in turn. */
  mockPartyVitals() {
    this.partyVitalsStep++;
    for (const player of playerStore.query(getAllEntities())) {
      const inCombat = this.partyVitalsStep % 3 !== 0;
      const downed = this.partyVitalsStep % 3 === 2;
      this.emit('setVitals', player.id, Math.floor(Math.random() * 101), Math.floor(Math.random() * 101), inCombat, downed);
      this.emit('setPlayerDistance', player.id, Math.floor(Math.random() * 300), Math.floor(Math.random() * 8));
    }
  }

  private partyVitalsStep = 0;

  /** A member's ping nearby, one in another place, and this player's own of their spot. */
  mockPartyPings() {
    this.emit('partyPing', 'Lydia', 'Bandit Chief', 42, 1, 'Whiterun');
    this.emit('partyPing', 'Lydia', '', -1, 0, 'Bleak Falls Barrow');
    this.emit('partyPing', '', '', 0, 0, 'Riverwood');
  }

  mockPartyTravel() {
    this.emit('partyTravel', 'Lydia', 'Riverwood');
  }

  mockQuestDesync() {
    this.emit('questDesync', 'Lydia', true, 'Bleak Falls Barrow, The Golden Claw, Dragon Rising, The Way of the Voice, Diplomatic Immunity', 3, 6);
  }

  tradeStart(playerId: number): void {
    this.emit('tradeInventory', JSON.stringify([{ name: 'Gold', count: 250, enchanted: false }, { name: 'Iron Sword', count: 1, enchanted: true }]));
    this.emit('trade', playerId, 'MockPartner', 0, '[]', '[]', false, false);
  }

  tradeOffer(picks: string): void {
    console.log(`trade offer ${picks}`);
  }

  tradeConfirm(): void {
    console.log('trade confirm');
  }

  tradeCancel(): void {
    console.log('trade cancel');
  }

  summonPlayer(playerId: number): void {
    const player = playerStore.query(getEntity(playerId));
    // The mock player declines every other time
    this.summonsDeclined = !this.summonsDeclined;
    setTimeout(() => this.emit('partyTeleportRefused', this.summonsDeclined ? 4 : 3, player?.name ?? '', 17), 500);
  }

  private summonsDeclined = false;

  answerPartySummon(summonerId: number, accept: boolean): void {
    console.log(`summons from ${summonerId}: ${accept ? 'go' : 'stay'}`);
  }

  setPartyQuestSharing(enabled: boolean, types: number): void {
    // The server tells the whole party, the leader too
    setTimeout(() => this.emit('partyQuestSharing', enabled, types), 200);
  }

  listShareableQuests(): void {
    const quests = [
      { id: 0x3372b, name: 'The Golden Claw' },
      { id: 0x2a49a, name: 'The Black Star' },
      { id: -16775168, name: 'A Light Plugin Quest' },
    ];
    setTimeout(() => this.emit('shareableQuests', JSON.stringify(quests)), 200);
  }

  shareQuest(questId: number): void {
    console.log(`offering quest ${questId} to the party`);
  }

  answerQuestShare(questId: number, accept: boolean): void {
    console.log(`quest ${questId} offer: ${accept ? 'accepted' : 'declined'}`);
  }

  answerQuestCatchUp(take: boolean): void {
    console.log(`leader's quest progress: ${take ? 'taken' : 'kept mine'}`);
  }

  repairQuestDesync(): void {
    console.log("quests repaired with the leader's stages");
  }

  mockQuestShareOffer() {
    this.emit('questShareOffer', 'Lydia', 'The Golden Claw', 0x3372b);
  }

  mockQuestCatchUp() {
    this.emit('questCatchUp', 'Lydia', 4);
  }

  mockPartySummon() {
    this.emit('partySummon', 2, 'Lydia');
  }

  mockPartyDoor() {
    this.emit('partyDoor', 'Lydia', 'Bleak Falls Barrow');
  }

  answerPartyDoor(follow: boolean): void {
    console.log(`party door: ${follow ? 'follow' : 'stay'}`);
  }

  mockLootRefused() {
    this.emit('lootRefused', 'Iron Sword', 1);
  }

  answerPartyTravel(travel: boolean): void {
    console.log(`travel together: ${travel ? 'yes' : 'no'}`);
  }

  inviteToPlayerMockParty(playerId: number) {
    playerStore.update(updateEntities(playerId, { invitedLocalPlayer: true }));
    this.emit('partyInviteReceived', playerId);
  }

  startPlayerMockParty(playerId: number) {
    playerStore.update(
      updateEntities(playerId, { hasOwnParty: true, isInGroup: true }),
    );
  }

  mockPlayerLeaveParty(playerId: number) {
    const player = playerStore.query(getEntity(playerId));

    playerStore.update(
      updateEntities(playerId, {
        hasOwnParty: false,
        invitedLocalPlayer: false,
        isInGroup: false,
      }),
    );
    if (this.connected) {
      if (player.isInGroup) {
        this.emit(
          'partyInfo',
          playerStore
            .query(getAllEntities())
            .filter(p => p.isInGroup)
            .map(p => p.id),
          this.localPlayerId,
        );
      } else if (player.hasOwnParty) {
        this.emit('partyInfo', [], -1);
      }
    }
  }

  updateMockDebugData() {
    const debugData = [
      Math.floor(Math.random() * 100),
      Math.floor(Math.random() * 100),
      Math.floor(Math.random() * 100),
      Math.floor((Math.random() / 4) * 100),
      Math.random() * 100,
      Math.random() * 100,
    ];
    this.emit('debugData', ...debugData);
    return debugData;
  }
}

export function mockSkyrimTogether() {
  (globalThis as any).skyrimtogether = new SkyrimtogetherMock();
}
