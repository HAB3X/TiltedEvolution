import { Injectable } from '@angular/core';
import { ReplaySubject } from 'rxjs';
import { environment } from 'src/environments/environment';
import { Command, CommandHandler } from './chat/commands';

export enum MessageTypes {
  SYSTEM_MESSAGE = 0,
  GLOBAL_CHAT = 1,
  PLAYER_DIALOGUE = 2,
  PARTY_CHAT = 3,
  LOCAL_CHAT = 4,
}

export interface ChatMessage {
  senderName?: string;
  translationParams?: Record<string, any>;
  content: string;
  type: MessageTypes;
}

const MUTED_KEY = 'muted_players';
const NAME_TAGS_KEY = 'name_tags';

@Injectable({
  providedIn: 'root',
})
export class ChatService {
  public messageList = new ReplaySubject<ChatMessage>();

  /**
   * Send chat message to the Server. Does not add anything to the local mesesage list.
   */
  public sendMessage(type: MessageTypes, content: string) {
    if (content.length === 0) {
      return;
    }

    if (content.length > environment.chatMessageLengthLimit) {
      this.pushSystemMessage('COMPONENT.CHAT.MESSAGE_TOO_LONG', {
        chatMessageLengthLimit: environment.chatMessageLengthLimit,
      });
      return;
    }

    if (content.startsWith(this.CommandHandler.COMMAND_PREFIX)) {
      this.CommandHandler.tryExecute(content);
      return;
    }

    skyrimtogether.sendMessage(type, content);
  }

  /**
   * Pushes a message to the chat window, without sending anything to the Server.
   */
  public pushMessage(message: ChatMessage) {
    this.messageList.next(message);
  }

  /**
   * For pushing translateable system messages to the chat window.
   */
  public pushSystemMessage(
    translationKey: string,
    params: Record<string, any> = undefined,
  ) {
    this.pushMessage({
      type: MessageTypes.SYSTEM_MESSAGE,
      content: translationKey,
      translationParams: params,
    });
  }

  private onMessageRecieved(
    type: MessageTypes,
    content: string,
    senderName: string | undefined = undefined,
  ): void {
    if (senderName && this.isMuted(senderName)) {
      return;
    }
    this.messageList.next({ type, content, senderName });
  }

  /** Players whose chat isn't shown (/mute, /unmute), by name, kept between sessions */
  private readonly muted = new Set<string>(
    JSON.parse(this.readStore(MUTED_KEY) ?? '[]') as string[],
  );

  public isMuted(name: string): boolean {
    return this.muted.has(name.toLowerCase());
  }

  public setMuted(name: string, muted: boolean): void {
    const key = name.toLowerCase();
    muted ? this.muted.add(key) : this.muted.delete(key);
    try {
      localStorage.setItem(MUTED_KEY, JSON.stringify([...this.muted]));
    } catch {
      // Not kept for the next session then; the mute still holds for this one
    }
  }

  private readStore(key: string): string | null {
    try {
      return localStorage.getItem(key);
    } catch {
      return null;
    }
  }

  public registerCommand(cmd: Command) {
    this.CommandHandler.register(cmd);
  }

  private CommandHandler: CommandHandler;

  private LocalChat: Command = {
    name: 'local',
    executor: async args => {
      const content = args.join(' ');
      this.sendMessage(MessageTypes.LOCAL_CHAT, content);
    },
  };

  private NameTags: Command = {
    name: 'nametags',
    executor: async () => {
      const enabled = this.readStore(NAME_TAGS_KEY) === 'false';
      try {
        localStorage.setItem(NAME_TAGS_KEY, String(enabled));
      } catch {
        // Only for this session then
      }
      skyrimtogether.setNameTags(enabled);
      this.pushSystemMessage(enabled ? 'COMPONENT.CHAT.NAME_TAGS_ON' : 'COMPONENT.CHAT.NAME_TAGS_OFF');
    },
  };

  /** The saved name tag choice, for the game when it starts */
  public get nameTagsEnabled(): boolean {
    return this.readStore(NAME_TAGS_KEY) !== 'false';
  }

  private Mute: Command = {
    name: 'mute',
    executor: async args => this.toggleMute(args.join(' '), true),
  };

  private Unmute: Command = {
    name: 'unmute',
    executor: async args => this.toggleMute(args.join(' '), false),
  };

  private toggleMute(name: string, muted: boolean): void {
    if (!name) {
      this.pushSystemMessage('COMPONENT.CHAT.MUTE_USAGE');
      return;
    }
    this.setMuted(name, muted);
    this.pushSystemMessage(muted ? 'COMPONENT.CHAT.MUTED' : 'COMPONENT.CHAT.UNMUTED', { name });
  }

  private PartyChat: Command = {
    name: 'party',
    executor: async args => {
      const content = args.join(' ');
      this.sendMessage(MessageTypes.PARTY_CHAT, content);
    },
  };

  constructor() {
    skyrimtogether.on('message', this.onMessageRecieved.bind(this));

    this.CommandHandler = new CommandHandler(this);
    this.CommandHandler.register(this.LocalChat);
    this.CommandHandler.register(this.PartyChat);
    this.CommandHandler.register(this.NameTags);
    this.CommandHandler.register(this.Mute);
    this.CommandHandler.register(this.Unmute);
  }
}
