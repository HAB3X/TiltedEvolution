import { Component, ChangeDetectionStrategy } from '@angular/core';
import {
  faArrowDown,
  faArrowUp,
  faWifi,
  IconDefinition,
} from '@fortawesome/free-solid-svg-icons';
import { combineLatest, map, Observable, startWith } from 'rxjs';
import { PerfData } from '../../models/perf-data';
import { ScreenCorner, SettingService } from 'src/app/services/setting.service';
import { Debug } from '../../models/debug';
import { ClientService } from '../../services/client.service';

const cornerClasses: Record<ScreenCorner, string> = {
  [ScreenCorner.TOP_LEFT]: 'top-left',
  [ScreenCorner.TOP_RIGHT]: 'top-right',
  [ScreenCorner.BOTTOM_RIGHT]: 'bottom-right',
  [ScreenCorner.BOTTOM_LEFT]: 'bottom-left',
};

@Component({
  selector: 'app-debug',
  templateUrl: './debug.component.html',
  styleUrls: ['./debug.component.scss'],
  changeDetection: ChangeDetectionStrategy.Eager,
  standalone: false,
})
export class DebugComponent {
  /* ### ICONS ### */
  faWifi: IconDefinition = faWifi;
  faArrowDown: IconDefinition = faArrowDown;
  faArrowUp: IconDefinition = faArrowUp;

  isShown$: Observable<boolean>;
  debugShown$: Observable<boolean>;
  perfShown$: Observable<boolean>;
  perfData$: Observable<PerfData | null>;
  debugData$: Observable<Debug>;
  cornerClass$: Observable<string>;

  constructor(
    private readonly client: ClientService,
    private readonly settingService: SettingService,
  ) {
    this.debugShown$ = this.settingService.settings.isDebugShown;
    this.perfShown$ = this.settingService.shown('perfOverlay');
    this.isShown$ = combineLatest([this.debugShown$, this.perfShown$]).pipe(map(([debug, perf]) => debug || perf));
    this.perfData$ = this.client.perfDataChange;
    this.debugData$ = this.client.debugDataChange.pipe(startWith(new Debug()));
    this.cornerClass$ = this.settingService.settings.debugAnchor.pipe(
      map(corner => cornerClasses[corner]),
    );
  }
}
