import { ChangeDetectionStrategy, Component } from '@angular/core';
import { combineLatest, timer } from 'rxjs';
import { map } from 'rxjs/operators';
import { ClientService } from '../../services/client.service';
import { PlayerListService } from '../../services/player-list.service';
import { Sound, SoundService } from '../../services/sound.service';

/** The open vote kick or ready check, its count and this player's answer; then its result for a few seconds. */
@Component({
  selector: 'app-vote',
  templateUrl: './vote.component.html',
  styleUrls: ['./vote.component.scss'],
  changeDetection: ChangeDetectionStrategy.Eager,
  standalone: false,
})
export class VoteComponent {
  public readonly vote$ = combineLatest([this.client.voteChange, timer(0, 1000)]).pipe(
    map(([vote]) => {
      if (!vote) {
        return null;
      }
      const isTarget = vote.kind === 'kick' && vote.targetId === this.client.localPlayerId;
      return {
        ...vote,
        starter: this.nameOf(vote.starterId),
        target: this.nameOf(vote.targetId),
        secondsLeft: Math.max(0, Math.ceil((vote.endsAt - Date.now()) / 1000)),
        isTarget,
        canAnswer: vote.state === 'open' && !vote.answered && !isTarget,
      };
    }),
  );

  public constructor(
    private readonly client: ClientService,
    private readonly playerList: PlayerListService,
    private readonly sound: SoundService,
  ) {}

  public answer(yes: boolean): void {
    this.sound.play(yes ? Sound.Ok : Sound.Cancel);
    this.client.answerVote(yes);
  }

  private nameOf(playerId: number): string {
    if (playerId === this.client.localPlayerId) {
      return this.client.nameChange.getValue();
    }
    // Not in the list yet: the server's id at least tells players apart
    return this.playerList.getPlayerById(playerId)?.name ?? `#${playerId}`;
  }
}
