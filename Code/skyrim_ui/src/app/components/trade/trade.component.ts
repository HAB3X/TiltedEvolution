import { ChangeDetectionStrategy, Component } from '@angular/core';
import { ClientService, TradeItem } from '../../services/client.service';
import { Sound, SoundService } from '../../services/sound.service';

/** A trade with a party member: pick amounts of your own stacks to offer, see theirs, and both confirm. */
@Component({
  selector: 'app-trade',
  templateUrl: './trade.component.html',
  styleUrls: ['./trade.component.scss'],
  changeDetection: ChangeDetectionStrategy.Eager,
  standalone: false,
})
export class TradeComponent {
  public readonly trade$ = this.client.tradeChange;
  public readonly inventory$ = this.client.tradeInventoryChange;
  /** How many of each of your stacks to offer, by index in the inventory list */
  public amounts: Record<number, number> = {};

  public constructor(
    private readonly client: ClientService,
    private readonly sound: SoundService,
  ) {}

  public setAmount(index: number, item: TradeItem, value: string): void {
    const amount = Math.max(0, Math.min(item.count, Math.floor(Number(value) || 0)));
    this.amounts[index] = amount;
  }

  public offer(): void {
    this.sound.play(Sound.Ok);
    const picks = Object.entries(this.amounts)
      .filter(([, count]) => count > 0)
      .map(([index, count]) => [Number(index), count] as [number, number]);
    this.client.offerTrade(picks);
  }

  public confirm(): void {
    this.sound.play(Sound.Ok);
    this.client.confirmTrade();
  }

  public close(): void {
    this.sound.play(Sound.Cancel);
    this.amounts = {};
    this.client.cancelTrade();
  }
}
