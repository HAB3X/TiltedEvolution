import { Injectable } from '@angular/core';
import { TranslocoService } from '@jsverse/transloco';
import { BehaviorSubject, combineLatest, distinctUntilChanged, map, Observable } from 'rxjs';
import { StoreService } from './store.service';

export enum FontSize {
  XS = 'xs',
  S = 's',
  M = 'm',
  L = 'l',
  XL = 'xl',
}

export enum ScreenCorner {
  TOP_LEFT = 0,
  TOP_RIGHT,
  BOTTOM_RIGHT,
  BOTTOM_LEFT,
}

export const autoHideTimerLengths = [1, 3, 5];

export const fontSizeToPixels: Record<FontSize, number> = {
  [FontSize.XS]: 10,
  [FontSize.S]: 12,
  [FontSize.M]: 16,
  [FontSize.L]: 20,
  [FontSize.XL]: 26,
};

class Setting<T> extends BehaviorSubject<T> {
  constructor(
    private readonly storeService: StoreService,
    private storeKey: string,
    defaultValue: T,
  ) {
    super(defaultValue);
  }

  public next(value: T) {
    this.storeService.set(this.storeKey, value);
    super.next(value);
  }
}

class SliderSetting extends Setting<number> {
  constructor(
    storeService: StoreService,
    storeKey: string,
    defaultValue: number,
  ) {
    const initialValue = storeService.getFloat(storeKey, defaultValue);
    super(storeService, storeKey, initialValue);
  }
}

class ToggleSetting extends Setting<boolean> {
  constructor(
    storeService: StoreService,
    storeKey: string,
    defaultValue: boolean,
  ) {
    const initialValue = storeService.getBool(storeKey, defaultValue);
    super(storeService, storeKey, initialValue);
  }
}

class SelectSetting<T extends string | number> extends Setting<T> {
  constructor(
    storeService: StoreService,
    storeKey: string,
    private options: T[],
    defaultValue: T,
  ) {
    const storedValue =
      typeof defaultValue === 'number'
        ? storeService.getFloat(storeKey, defaultValue)
        : storeService.get(storeKey, defaultValue);
    const initialValue = options.includes(storedValue)
      ? storedValue
      : defaultValue;
    super(storeService, storeKey, initialValue);
  }

  public next(value: T) {
    if (this.options.includes(value)) {
      super.next(value);
    }
  }
}

@Injectable({
  providedIn: 'root',
})
export class SettingService {
  private readonly languageValues: string[] = Object.values(
    this.translocoService.getAvailableLangs(),
  ).map(lang => lang.id);
  private readonly fontSizeValues = Object.values(FontSize);
  private readonly screenCornerValues = Object.values(
    ScreenCorner,
  ) as ScreenCorner[];
  private readonly autoHideTimeValues = autoHideTimerLengths;

  public settings = {
    volume: new SliderSetting(this.storeService, 'audio_volume', 0.5),
    muted: new ToggleSetting(this.storeService, 'audio_muted', false),
    language: new SelectSetting(
      this.storeService,
      'language',
      this.languageValues,
      this.translocoService.getDefaultLang(),
    ),
    fontSize: new SelectSetting(
      this.storeService,
      'font_size',
      this.fontSizeValues,
      FontSize.M,
    ),
    isPartyShown: new ToggleSetting(this.storeService, 'party_isShown', true),
    autoHideParty: new ToggleSetting(
      this.storeService,
      'party_autoHide',
      false,
    ),
    autoHideTime: new SelectSetting(
      this.storeService,
      'party_autoHideTime',
      this.autoHideTimeValues,
      this.autoHideTimeValues[0],
    ),
    partyAnchor: new SelectSetting(
      this.storeService,
      'party_anchor',
      this.screenCornerValues,
      ScreenCorner.TOP_LEFT,
    ),
    partyAnchorOffsetX: new SliderSetting(
      this.storeService,
      'party_anchor_offset_x',
      0,
    ),
    partyAnchorOffsetY: new SliderSetting(
      this.storeService,
      'party_anchor_offset_y',
      3,
    ),
    isDebugShown: new ToggleSetting(this.storeService, 'debug_isShown', false),
    /** Hides every on-screen multiplayer element below at once, keeping the player's own choices for when it's off. */
    immersiveMode: new ToggleSetting(this.storeService, 'immersive_mode', false),
    /** The party bar's chat lines and notifications when a member enters combat or goes down. */
    partyAlerts: new ToggleSetting(this.storeService, 'party_alerts', true),
    /** The game's waypoint on other players' pings; this player can still ping. */
    pingMarkers: new ToggleSetting(this.storeService, 'ping_markers', true),
    /** The chat line saying who pinged what. */
    pingMessages: new ToggleSetting(this.storeService, 'ping_messages', true),
    /** Names above other players; the same key the /nametags chat command always used. */
    nameTags: new ToggleSetting(this.storeService, 'name_tags', true),
    /** Frame rate, frame time, ping, synced actors and Skyrim Together's own cost per frame. */
    perfOverlay: new ToggleSetting(this.storeService, 'perf_overlay', false),
    debugAnchor: new SelectSetting(
      this.storeService,
      'debug_anchor',
      this.screenCornerValues,
      ScreenCorner.TOP_LEFT,
    ),
  };

  /** The on-screen elements immersive mode hides. */
  public readonly immersionSettings = [
    'isPartyShown',
    'partyAlerts',
    'pingMarkers',
    'pingMessages',
    'nameTags',
    'perfOverlay',
  ] as const;

  /** Whether an on-screen element shows: the player's choice, unless immersive mode hides it. */
  public shown(name: (typeof this.immersionSettings)[number]): Observable<boolean> {
    return combineLatest([this.settings.immersiveMode, this.settings[name]]).pipe(
      map(([immersive, value]) => !immersive && value),
      distinctUntilChanged(),
    );
  }

  /** shown, now. */
  public isShown(name: (typeof this.immersionSettings)[number]): boolean {
    return !this.settings.immersiveMode.getValue() && this.settings[name].getValue();
  }

  constructor(
    private readonly storeService: StoreService,
    private readonly translocoService: TranslocoService,
  ) {
    this.settings.language.subscribe(lang =>
      translocoService.setActiveLang(lang),
    );
  }
}
