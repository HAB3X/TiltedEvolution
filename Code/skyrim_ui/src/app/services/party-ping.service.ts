import { Injectable } from '@angular/core';
import { TranslocoService } from '@jsverse/transloco';
import { compassKey, pingKeys } from '../utils/party-bar';
import { ChatService } from './chat.service';
import { ClientService } from './client.service';
import { SettingService } from './setting.service';

/** Party pings (F3 in game): a chat line saying who pinged what, and how far and which way it is. */
@Injectable({
  providedIn: 'root',
})
export class PartyPingService {
  constructor(
    clientService: ClientService,
    chatService: ChatService,
    translocoService: TranslocoService,
    settingService: SettingService,
  ) {
    clientService.partyPingChange.subscribe(ping => {
      // Others' pings only; this player's own line says the ping went out
      if (ping.player && !settingService.isShown('pingMessages')) {
        return;
      }
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
