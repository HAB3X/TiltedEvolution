import { Injectable } from '@angular/core';
import { ClientService } from './client.service';
import { PopupNotificationService } from './popup-notification.service';

/** Travel together: when the party leader fast travels, asks whether to go with them. */
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
  }
}
