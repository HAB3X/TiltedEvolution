import { Injectable } from '@angular/core';
import { TranslocoService } from '@jsverse/transloco';
import { compassKey, pingKeys } from '../utils/party-bar';
import { ChatService } from './chat.service';
import { ClientService } from './client.service';

/** Party pings (F3 in game): a chat line saying who pinged what, and how far and which way it is. */
@Injectable({
  providedIn: 'root',
})
export class PartyPingService {
  constructor(
    clientService: ClientService,
    chatService: ChatService,
    translocoService: TranslocoService,
  ) {
    clientService.partyPingChange.subscribe(ping => {
      const { line, spot } = pingKeys(ping);
      const direction = translocoService.translate(`COMPONENT.PARTY_BAR.DIRECTION.${compassKey(ping.direction)}`);
      chatService.pushSystemMessage(`SERVICE.PARTY_PING.${line}`, {
        name: ping.player,
        target: spot ? translocoService.translate(`SERVICE.PARTY_PING.${spot}`) : ping.target,
        distance: translocoService.translate('COMPONENT.PARTY_BAR.DISTANCE', { meters: ping.meters, direction }),
        place: ping.place,
      });
    });
  }
}
