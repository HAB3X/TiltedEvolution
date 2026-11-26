import { Injectable } from '@angular/core';
import {
  faDoorOpen,
  faHandshakeSimple,
  faPersonWalkingArrowRight,
  faRoute,
  faScroll,
  faUserGroup,
} from '@fortawesome/free-solid-svg-icons';
import { Subject } from 'rxjs';
import { PopupNotification } from '../models/popup-notification';
import { Sound, SoundService } from './sound.service';

@Injectable({
  providedIn: 'root',
})
export class PopupNotificationService {
  private message = new Subject<PopupNotification>();
  public message$ = this.message.asObservable();
  private messagesCleared = new Subject<void>();
  public messagesCleared$ = this.messagesCleared.asObservable();

  constructor(private readonly soundService: SoundService) {}

  public addMessage(notification: PopupNotification) {
    this.message.next(notification);
    this.soundService.play(Sound.Focus);
  }

  public addPartyInvite(from: string, callback: () => void) {
    this.addMessage({
      messageKey: 'SERVICE.PLAYER_LIST.PARTY_INVITE',
      messageParams: { from },
      icon: faHandshakeSimple,
      duration: 30000,
      actions: [
        {
          nameKey: 'COMPONENT.NOTIFICATIONS.ACCEPT',
          callback,
        },
      ],
    });
  }

  /** The party leader fast travelled: travel with them? Goes away after 30 seconds, like a party invite. */
  public addPartyTravel(leader: string, place: string, answer: (travel: boolean) => void) {
    this.addMessage({
      messageKey: place ? 'SERVICE.TRAVEL_TOGETHER.QUESTION' : 'SERVICE.TRAVEL_TOGETHER.QUESTION_NO_PLACE',
      messageParams: { leader, place },
      icon: faRoute,
      duration: 30000,
      actions: [
        { nameKey: 'SERVICE.TRAVEL_TOGETHER.YES', callback: () => answer(true) },
        { nameKey: 'SERVICE.TRAVEL_TOGETHER.NO', callback: () => answer(false) },
      ],
    });
  }

  /** A party member went through a load door: follow them? Goes away after 30 seconds, like a party invite. */
  public addPartyDoor(member: string, place: string, answer: (follow: boolean) => void) {
    this.addMessage({
      messageKey: place ? 'SERVICE.PARTY_DOOR.QUESTION' : 'SERVICE.PARTY_DOOR.QUESTION_NO_PLACE',
      messageParams: { member, place },
      icon: faDoorOpen,
      duration: 30000,
      actions: [
        { nameKey: 'SERVICE.TRAVEL_TOGETHER.YES', callback: () => answer(true) },
        { nameKey: 'SERVICE.TRAVEL_TOGETHER.NO', callback: () => answer(false) },
      ],
    });
  }

  /** A party member asks this player to come to them. Goes away after 30 seconds, as long as the server waits. */
  public addPartySummon(summoner: string, answer: (accept: boolean) => void) {
    this.addMessage({
      messageKey: 'SERVICE.PARTY_TELEPORT.SUMMONED',
      messageParams: { name: summoner },
      icon: faPersonWalkingArrowRight,
      duration: 30000,
      actions: [
        { nameKey: 'SERVICE.TRAVEL_TOGETHER.YES', callback: () => answer(true) },
        { nameKey: 'SERVICE.TRAVEL_TOGETHER.NO', callback: () => answer(false) },
      ],
    });
  }

  /** A party member offers one of their quests. Goes away after 30 seconds, like a party invite. */
  public addQuestShareOffer(player: string, quest: string, answer: (accept: boolean) => void) {
    this.addMessage({
      messageKey: 'SERVICE.QUEST_SHARING.OFFER',
      messageParams: { name: player, quest },
      icon: faScroll,
      duration: 30000,
      actions: [
        { nameKey: 'SERVICE.QUEST_SHARING.ACCEPT', callback: () => answer(true) },
        { nameKey: 'SERVICE.QUEST_SHARING.DECLINE', callback: () => answer(false) },
      ],
    });
  }

  /** On joining a party: take the leader's progress in the shared quests, or keep this player's own? */
  public addQuestCatchUp(leader: string, quests: number, answer: (take: boolean) => void) {
    this.addMessage({
      messageKey: 'SERVICE.QUEST_SHARING.CATCH_UP',
      messageParams: { name: leader, count: quests },
      icon: faScroll,
      duration: 60000,
      actions: [
        { nameKey: 'SERVICE.QUEST_SHARING.TAKE_PROGRESS', callback: () => answer(true) },
        { nameKey: 'SERVICE.QUEST_SHARING.KEEP_MINE', callback: () => answer(false) },
      ],
    });
  }

  /** The member's shared quests keep differing from the leader's: take the leader's stages of them? */
  public addQuestRepair(leader: string, quests: number, repair: () => void) {
    this.addMessage({
      messageKey: 'SERVICE.QUEST_DESYNC.REPAIR_OFFER',
      messageParams: { name: leader, count: quests },
      icon: faScroll,
      duration: 60000,
      actions: [
        { nameKey: 'SERVICE.QUEST_DESYNC.REPAIR', callback: repair },
        { nameKey: 'COMPONENT.NOTIFICATIONS.DISMISS', callback: () => {} },
      ],
    });
  }

  public addPartyHint(onOpenGuide: () => void, onDontShow: () => void) {
    this.addMessage({
      messageKey: 'SERVICE.PARTY_HINT.MESSAGE',
      icon: faUserGroup,
      duration: 15000,
      actions: [
        {
          nameKey: 'COMPONENT.NOTIFICATIONS.OPEN_GUIDE',
          callback: onOpenGuide,
        },
        {
          nameKey: 'COMPONENT.NOTIFICATIONS.DISMISS',
          // Clicking anywhere on the popup removes it
          callback: () => {},
        },
        {
          nameKey: 'COMPONENT.NOTIFICATIONS.DONT_SHOW',
          callback: onDontShow,
        },
      ],
    });
  }

  public clearMessages() {
    this.messagesCleared.next();
  }
}
