import { Injectable } from '@angular/core';
import { ClientService } from './client.service';
import { PopupNotificationService } from './popup-notification.service';

/** Travel together: when the party leader fast travels, or a member goes through a load door, asks whether to follow. */
@Injectable({
  providedIn: 'root',
})
export class TravelTogetherService {
  constructor(
    clientService: ClientService,
    popupNotificationService: PopupNotificationService,
  ) {
    clientService.partyTravelChange.subscribe(({ leader, place }) =>
      popupNotificationService.addPartyTravel(leader, place, travel =>
        clientService.answerPartyTravel(travel),
      ),
    );
    clientService.partyDoorChange.subscribe(({ member, place }) =>
      popupNotificationService.addPartyDoor(member, place, follow =>
        clientService.answerPartyDoor(follow),
      ),
    );
  }
}
