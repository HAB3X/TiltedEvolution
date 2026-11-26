import { Injectable } from '@angular/core';
import { ClientService } from './client.service';
import { PopupNotificationService } from './popup-notification.service';
import { ChatService } from './chat.service';
import { teleportRefusalKey } from '../utils/party-bar';

/** Travel together: when the party leader fast travels, or a member goes through a load door, asks whether to follow. */
@Injectable({
  providedIn: 'root',
})
export class TravelTogetherService {
  constructor(
    clientService: ClientService,
    popupNotificationService: PopupNotificationService,
    chatService: ChatService,
  ) {
    clientService.partyTravelChange.subscribe(({ leader, place }) =>
      popupNotificationService.addPartyTravel(leader, place, travel =>
        clientService.answerPartyTravel(travel),
      ),
    );
    clientService.partySummonChange.subscribe(({ summonerId, summoner }) =>
      popupNotificationService.addPartySummon(summoner, accept =>
        clientService.answerPartySummon(summonerId, accept),
      ),
    );
    clientService.partyTeleportRefusedChange.subscribe(({ reason, player, secondsLeft }) =>
      chatService.pushSystemMessage(`SERVICE.PARTY_TELEPORT.${teleportRefusalKey(reason)}`, { name: player, seconds: secondsLeft }),
    );
    clientService.partyDoorChange.subscribe(({ member, place }) =>
      popupNotificationService.addPartyDoor(member, place, follow =>
        clientService.answerPartyDoor(follow),
      ),
    );
  }
}
