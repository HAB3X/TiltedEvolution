import { Injectable } from '@angular/core';
import { TranslocoService } from '@jsverse/transloco';
import { ChatService } from './chat.service';
import { ClientService } from './client.service';
import { PopupNotificationService } from './popup-notification.service';

/**
 * The party's quest desync check: says in chat which shared quests differ from the leader's (or a member's), and offers
 * a member to repair them with the leader's stages.
 */
@Injectable({
  providedIn: 'root',
})
export class QuestDesyncService {
  constructor(
    chatService: ChatService,
    clientService: ClientService,
    translocoService: TranslocoService,
    popupNotificationService: PopupNotificationService,
  ) {
    clientService.questDesyncChange.subscribe(({ otherPlayer, otherIsLeader, quests, more, repairable }) => {
      const named = more > 0 ? translocoService.translate('SERVICE.QUEST_DESYNC.MORE', { quests, count: more }) : quests;
      chatService.pushSystemMessage(otherIsLeader ? 'SERVICE.QUEST_DESYNC.WITH_LEADER' : 'SERVICE.QUEST_DESYNC.WITH_MEMBER', {
        name: otherPlayer,
        quests: named,
      });
      if (otherIsLeader && repairable > 0) {
        popupNotificationService.addQuestRepair(otherPlayer, repairable, () => clientService.repairQuestDesync());
      }
    });
  }
}
