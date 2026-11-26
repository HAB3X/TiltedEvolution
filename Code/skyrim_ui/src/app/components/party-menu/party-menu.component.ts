import { ChangeDetectionStrategy, Component } from '@angular/core';
import { Observable, pluck, startWith } from 'rxjs';
import { filter, map } from 'rxjs/operators';
import { Player } from 'src/app/models/player';
import { ClientService } from 'src/app/services/client.service';
import { GroupService } from 'src/app/services/group.service';
import { LoadingService } from 'src/app/services/loading.service';
import { PlayerListService } from 'src/app/services/player-list.service';
import { maskFromQuestTypes, PartyQuestSharing, QUEST_TYPES, questTypesFromMask } from 'src/app/models/shared-quests';

@Component({
  selector: 'app-party-menu',
  templateUrl: './party-menu.component.html',
  styleUrls: ['./party-menu.component.scss'],
  changeDetection: ChangeDetectionStrategy.OnPush,
  standalone: false,
})
export class PartyMenuComponent {
  isLoading$ = this.loadingService.getLoading();
  members$ = this.groupService.selectMembers();
  memberCount$ = this.groupService.selectMembersLength(true);
  group$ = this.groupService.group.asObservable();
  isLaunchPartyDisabled$: Observable<boolean>;
  invitations$: Observable<Player[]>;
  isPartyLeader$: Observable<boolean>;
  questSharing$ = this.clientService.partyQuestSharingChange.asObservable();
  shareableQuests$ = this.clientService.shareableQuestsChange.asObservable();
  readonly questTypes = QUEST_TYPES;
  /** Whether the list of this player's quests to offer is open */
  showShareable = false;
  /** The quests offered since the list was opened */
  offered = new Set<number>();

  constructor(
    private readonly groupService: GroupService,
    private readonly loadingService: LoadingService,
    private readonly playerListService: PlayerListService,
    private readonly clientService: ClientService,
  ) {
    this.invitations$ = this.playerListService.playerList.asObservable().pipe(
      filter(playerlist => !!playerlist),
      pluck('players'),
      map(players => players.filter(player => player.hasInvitedLocalPlayer)),
      startWith([]),
    );
    this.isPartyLeader$ = this.groupService.group
      .asObservable()
      .pipe(
        map(
          group =>
            group.isEnabled && group.owner == this.clientService.localPlayerId,
        ),
      );
    this.isLaunchPartyDisabled$ = this.groupService
      .selectMembersLength(false)
      .pipe(map(count => count > 1));
  }

  public launchParty() {
    this.groupService.launch();
  }

  public leave() {
    this.groupService.leave();
  }

  public teleportToPlayer(playerId: number) {
    this.clientService.teleportToPlayer(playerId);
  }

  public acceptPartyInvite(inviterId: number) {
    this.playerListService.acceptPartyInvite(inviterId);
  }

  public kickMember(playerId: number) {
    this.clientService.kickPartyMember(playerId);
  }

  public isShared(sharing: PartyQuestSharing, type: string): boolean {
    return questTypesFromMask(sharing.types).includes(type);
  }

  public setSharingEnabled(sharing: PartyQuestSharing, enabled: boolean) {
    this.clientService.setPartyQuestSharing(enabled, sharing.types);
  }

  public toggleType(sharing: PartyQuestSharing, type: string) {
    const types = new Set(questTypesFromMask(sharing.types));
    if (!types.delete(type)) types.add(type);
    this.clientService.setPartyQuestSharing(sharing.enabled, maskFromQuestTypes(types));
  }

  public listShareableQuests() {
    this.showShareable = true;
    this.offered = new Set();
    this.clientService.listShareableQuests();
  }

  public shareQuest(questId: number) {
    this.offered = new Set(this.offered).add(questId);
    this.clientService.shareQuest(questId);
  }

  public changeLeader(playerId: number) {
    this.clientService.changePartyLeader(playerId);
  }
}
