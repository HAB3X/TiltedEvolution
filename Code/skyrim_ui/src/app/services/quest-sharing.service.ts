import { Injectable } from '@angular/core';
import { ClientService } from './client.service';
import { PopupNotificationService } from './popup-notification.service';

/** Asks about quests a party member offers, and on joining a party whether to take the leader's quest progress. */
@Injectable({
  providedIn: 'root',
})
export class QuestSharingService {
  constructor(clientService: ClientService, popupNotificationService: PopupNotificationService) {
    clientService.questShareOfferChange.subscribe(({ player, quest, questId }) =>
      popupNotificationService.addQuestShareOffer(player, quest, accept => clientService.answerQuestShare(questId, accept)),
    );
    clientService.questCatchUpChange.subscribe(({ leader, quests }) =>
      popupNotificationService.addQuestCatchUp(leader, quests, take => clientService.answerQuestCatchUp(take)),
    );
  }
}
