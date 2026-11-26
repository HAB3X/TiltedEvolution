import { Injectable } from '@angular/core';
import { ChatService } from './chat.service';
import { ClientService } from './client.service';

/** Says in chat when another player took the same container items first and this player's were given back. */
@Injectable({
  providedIn: 'root',
})
export class LootService {
  constructor(chatService: ChatService, clientService: ClientService) {
    clientService.lootRefusedChange.subscribe(({ item, count }) => {
      chatService.pushSystemMessage('SERVICE.LOOT.TAKEN_FIRST', { item, count });
    });
  }
}
