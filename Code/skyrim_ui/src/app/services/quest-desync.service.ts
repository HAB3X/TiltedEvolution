import { Injectable } from '@angular/core';
import { TranslocoService } from '@jsverse/transloco';
import { ChatService } from './chat.service';
import { ClientService } from './client.service';

/** The party's quest desync check: says in chat which shared quests differ from the leader's (or a member's). */
@Injectable({
  providedIn: 'root',
})
export class QuestDesyncService {
  constructor(
    chatService: ChatService,
    clientService: ClientService,
    translocoService: TranslocoService,
  ) {
    clientService.questDesyncChange.subscribe(({ otherPlayer, otherIsLeader, quests, more }) => {
      const named = more > 0 ? translocoService.translate('SERVICE.QUEST_DESYNC.MORE', { quests, count: more }) : quests;
      chatService.pushSystemMessage(otherIsLeader ? 'SERVICE.QUEST_DESYNC.WITH_LEADER' : 'SERVICE.QUEST_DESYNC.WITH_MEMBER', {
        name: otherPlayer,
        quests: named,
      });
    });
  }
}
