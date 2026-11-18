# Fixes from community forks

Client-side commits from community forks of Skyrim Together Reborn, applied onto v1.8.4 in date order with their original authors. Each commit keeps a `(cherry picked from commit ...)` line pointing at the original.

| Date | Author | Change |
|---|---|---|
| 2026-09-01 | Colin | fix: align Papyrus VM vtable for Skyrim VR |
| 2026-09-03 | Sanford Leach | build: pin xmake to 2.9.8 in the server Dockerfile |
| 2026-09-04 | MrMartexX | Cover process request lifecycle quiescence |
| 2026-09-03 | Sanford Leach | fix(ui): the party menu said "Skyrim" for every outdoor player |
| 2026-09-03 | Sanford Leach | build(deps): force every tiltedcore require to v0.2.9 |
| 2026-09-06 | MrMartexX | Test signal-driven runtime bootstrap ordering |
| 2026-09-06 | MrMartexX | Prove bootstrap bind fencing and allocation fail-closed |
| 2026-09-08 | Colin | fix: interleave VR VM slots using binary comparison |
| 2026-09-14 | MrMartexX | Fix naked NPC inventory recreation safety |
| 2026-09-14 | Colin | fix: restore Games/TES.h include in MagicService |
| 2026-09-14 | Colin | fix: keep VR message boxes visible without pausing the game |
| 2026-09-15 | Sergio Rayo | Include <cstring> for std::strlen in SubtitleManager (#854 merge follow-up) |
| 2026-09-22 | Sergio Rayo | Add sit and sleep state helpers to ActorState |
| 2026-09-22 | Sergio Rayo | Reset the stale seated pose of remote actors on replay |
| 2026-09-23 | MrMartexX | test native load adapter owner contract |
| 2026-09-30 | MostExcellent | RE: add IPostAnimationChannelUpdateFunctor::DoPostAnimationChannelUpdate, rename Actor::unk78 (correctly) to postAnimationChannelUpdateFunctor |
| 2026-09-30 | MostExcellent | remove unused SAVE_STUFF and related code, Actor::Save_Reversed would not compile anyway. |
| 2026-09-30 | MostExcellent | remove unused externs from DebugService.cpp |
| 2026-09-30 | MostExcellent | Remove commented SetLeveledCreature hook lines, unused RealSetLeveledCreature and the now unused TU scope TSetLeveledCreature typedef |
| 2026-09-30 | MostExcellent | remove a bunch of magic-related undefined function declarations + fix IsInvisibilityEffect typo |
| 2026-09-30 | MostExcellent | Remove no-op FindTargets and GetPerkRank hooks from MagicTarget.cpp |
| 2026-09-29 | MostExcellent | Remove dead code relating to old animation RE investigations. RE documentation TODO if desired. |
| 2026-09-29 | David Glover | Undo unintentional hkEventContext changes |
| 2026-09-30 | MostExcellent | fix: GamePtr move ctor initialization before swap, make operators able to be used for const T ptr or ref, make bool operator explicit, move assign noexcept |
| 2026-10-03 | Wookie1411 | M2 step 1: log load-door activations by the local player (untested) |
| 2026-10-03 | Wookie1411 | M2 step 2: hold load doors while in a party with a partner (untested) |
| 2026-10-03 | Wookie1411 | Fast travel step 1: log answers to the fast-travel prompt (untested) |
| 2026-10-03 | MostExcellent | fix DesiredTarget not initialized in MagicService::OnSpellCastEvent, preventing unnecessary logging and incorrect targets being sent if the uninitialized value does happen to match a valid ID |
| 2026-10-04 | TheSporkBard | Target Enderal Special Edition in launcher |
| 2026-10-04 | TheSporkBard | Allow Enderal plugin layout during smoke testing |
| 2026-10-05 | GhostWriter42 | Sync persistent quest-item pickups to party members |
| 2026-01-02 | liangchen | prevent teammates from syncing QuestStop events for incomplete quests |
| 2026-01-02 | liangchen | remove redundant check |
| 2026-10-05 | GhostWriter42 | Add TESQuest::IsCompleted() and restore the completed-quest check |
| 2026-10-05 | GhostWriter42 | fix(party): null-check WorldEncountersEnabled global on party info |
| 2026-10-05 | GhostWriter42 | fix(beastform): guard OnBeastFormChange against missing player entity |
| 2026-10-05 | GhostWriter42 | fix(party): client PartyService init leader id, clear player list on disconnect |
| 2026-10-05 | GhostWriter42 | fix(party): client drop consumed invite on party join |
| 2026-10-11 | mailamea10 | launcher: pre-bind System32 dwmapi.dll to avoid shadowing by extra DLL directories |
| 2026-10-14 | Mariagrazia Grasso | fix(sync): preserve factions while NPCs load |
| 2026-10-14 | Mariagrazia Grasso | fix(sync): skip unavailable actors in weapon state polling |
| 2026-10-14 | Mariagrazia Grasso | fix(sync): discard stale client state on disconnect |
| 2026-10-14 | Mariagrazia Grasso | fix(sync): clear queued effects after disconnect |
| 2026-10-14 | Mariagrazia Grasso | fix(sync): activate quests on remote stage updates |
| 2026-10-14 | Mariagrazia Grasso | fix(sync): remove custom actors before form binding |
| 2026-10-14 | Mariagrazia Grasso | fix(sync): clear animation queues on disconnect |
| 2026-10-14 | Mariagrazia Grasso | fix(client): avoid null iteration for empty game arrays |
| 2026-10-15 | Mariagrazia Grasso | fix(sync): guard custom enchantment array resize |
| 2026-10-15 | Mariagrazia Grasso | fix(sync): reject unresolved enchantment effects |
| 2026-10-15 | Mariagrazia Grasso | fix(sync): restore lazy summon command target |
| 2026-10-15 | Mariagrazia Grasso | fix(sync): guard unavailable cells during grid shifts |
| 2026-10-15 | Mariagrazia Grasso | fix(sync): retry actor assignment after cell load |
| 2026-10-15 | Mariagrazia Grasso | fix(sync): guard the initial player cell lookup |
| 2026-10-15 | Mariagrazia Grasso | fix(sync): simplify pending actor assignment queue |
| 2026-10-15 | Mariagrazia Grasso | fix(quest): guard empty player objective entries |
| 2026-10-15 | Mariagrazia Grasso | fix(sync): guard actor discovery before player load |
| 2026-10-15 | Mariagrazia Grasso | fix(sync): skip player updates before player load |
| 2026-10-15 | Mariagrazia Grasso | fix(sync): defer auth until player is ready |
| 2026-10-15 | Mariagrazia Grasso | fix(sync): use effective actor cell in movement snapshots |
| 2026-10-15 | Mariagrazia Grasso | fix(sync): use effective cell for NPC assignment |
| 2026-10-15 | Mariagrazia Grasso | fix(inventory): suppress duplicate NPC pickup events |
| 2026-10-16 | Daniil Zakharov | Fix scene type redefinition after rebase onto dev |
| 2026-10-20 | absol89 | feat: player name tags (port of v1.8.1 NameTagService onto dev) |
| 2026-10-20 | absol89 | feat: player UI filters for name tags, stranger names and invites |
| 2026-10-20 | absol89 | feat: eye overrides names for party members too, "Known" column title |
| 2026-10-21 | absol89 | fix: "Global stranger names" checked hides non-party names, invites included |
| 2026-10-21 | absol89 | fix: unknown names show as italic Stranger#<id> instead of #<id> |
| 2026-10-21 | absol89 | feat: provide the trade invite policy from the player UI settings |
| 2026-10-21 | absol89 | refactor: keep name tag edits clear of player trading's lines |
| 2026-10-24 | absol89 | Fix a crash when a party member's waypoint has no worldspace |
| 2026-10-24 | gherman7755 | Keep the owner's actor values when conforming a leveled NPC |
| 2026-10-24 | absol89 | Lock ExtraDataList through the game's reader/writer lock |
| 2026-10-30 | Fallout Together Reborn Contributors | Client: add TP_NO_OVERLAY_RENDER diagnostic switch |
| 2026-10-30 | Fallout Together Reborn Contributors | Quests: start stopped quests before applying a remote stage |
| 2026-10-30 | Fallout Together Reborn Contributors | Memory: hook realloc as well |
| 2026-10-30 | Fallout Together Reborn Contributors | Memory: send foreign blocks back to the CRT |
| 2026-10-30 | barneym | Continue past thread naming exceptions in the launcher |
| 2026-10-31 | Claude | Let side-by-side test clients use separate gamepads and F4 |
| 2026-10-31 | Claude | Find the game's XInput import instead of assuming xinput1_3 |
| 2026-10-31 | barneym | Repaint a remote player's face after its head is rebuilt |
| 2026-10-31 | Claude | Add a follow-the-leader mode for solo testing |
| 2026-10-31 | SupperCraft5000 | Track the package sources |
| 2026-10-31 | SupperCraft5000 | Starter: connect over the Steam network |
| 2026-10-31 | SupperCraft5000 | Starter: make room for the Steam option |
| 2026-10-31 | Claude | Log follow mode's distance from the leader |
| 2026-10-31 | Claude | Find Papyrus natives whatever case the engine stored their names in |
| 2026-10-31 | Claude | Keep NPCs animating for others when their owner looks away |
| 2026-10-31 | Claude | Add animation sync logging behind ST_ANIM_LOG |
| 2026-10-31 | Claude | Turn the follower toward the leader |
| 2026-10-31 | Claude | Fix the Windows build of the animation sync logging |
| 2026-10-31 | Claude | Log when a client joins a party |
| 2026-10-31 | Claude | Stop turning the follower toward the leader |
| 2026-10-31 | Claude | Stop actors that can't sheathe flooding other clients |
| 2026-10-31 | Claude | Fix stuck weapon states, door loops and lost gear on copies |
| 2026-10-19 | absol89 | Fix crash when a launched projectile can't be found |
| 2026-10-18 | absol89 | Don't equip unresolved spells or shouts on spawned actors |
| 2026-10-28 | absol89 | Fix a crash when an activated object is deleted before its activation is handled |
| 2026-10-24 | absol89 | Don't give summons death items |
| 2026-10-03 | absol89 | fix: restore torso-less remote player bodies in validation build |
| 2026-10-05 | GhostWriter42 | Fire BeastFormChangeEvent on beast-form leave after the race reverts |
| 2026-10-17 | absol89 | Don't party-sync the Riften North Gate guard scene quest |
| 2026-11-02 | absol89 | The console follows SkyrimTogetherTweaks.ini |
| 2026-11-02 | Claude | Ship the Opus licence notice with the client |
| 2026-11-02 | absol89 | Stop sending truncated client messages |
| 2026-11-03 | Julien QUENTIN | Show server addresses in the public server list |
| 2026-11-04 | Claude | Don't hand out a remote actor the game failed to place |
| 2026-11-04 | Claude | Log Characters that end up with form ID 0 |
| 2026-11-04 | Claude | Don't move remote actors past the edge of the loaded area |
| 2026-11-04 | Claude | Log actor and player placement around assignment and removal |
| 2026-11-04 | Claude | Revert "Don't move remote actors past the edge of the loaded area" |
| 2026-11-04 | Claude | Don't block in the crash handler |
| 2026-11-04 | Claude | Don't spawn dead actors that are disabled locally |
| 2026-11-04 | Claude | Don't restart completed run once quests from party updates |
| 2026-11-04 | Claude | Cut party UI redraws while a party member is in combat |
| 2026-11-05 | Claude | Don't teleport remote actors into a worldspace the player isn't in |
| 2026-11-04 | absol89 | Show other players with Detect Life and keep it to the caster |
| 2026-11-04 | absol89 | Make poisons apply to other players in PvP |
| 2026-11-05 | jotagalindo | Make the F2 UI usable on ultrawide screens |
| 2026-11-05 | jotagalindo | Point TiltedUI at the ultrawide overlay fix for testing |
| 2026-11-05 | absol89 | Declare the world position of NiAVObject |
| 2026-11-08 | NutyoPR | Respawn the puppet for any non-playable (beast) race, not just vanilla werewolf/vampire lord |
| 2026-11-11 | the3venthoriz0n | server: add DateTime::AdvanceHours and HoursUntil with tests |
| 2026-11-14 | Claude | Add a build_version option so a branch build can report a release version |
| 2026-11-14 | the3venthoriz0n | Skip destroyed magic items in combat scoring instead of crashing |
| 2026-11-14 | the3venthoriz0n | CombatMagicItemGuard: also reject destroyed forms |
| 2026-11-14 | the3venthoriz0n | Skip combat magic items for remotely owned actors; shared crash guard config |
| 2026-11-15 | Claude | Let dead NPCs ragdoll on every client |
| 2026-11-15 | Claude | Run the game's actor update for dead remote NPCs |
| 2026-11-09 | Zenderal | fix(client): give bow aim controllers a fresh weapon bone |
| 2026-11-09 | friendlycocktails-lgtm | fix(client): don't hook ID 37448, it is GetSubmergedLevel, not attribute regen |
| 2026-11-09 | Zenderal | fix(client): don't sync actions that have no actor or no BGSAction |
| 2026-11-16 | Claude | Let non-host players loot corpses of remotely killed NPCs |
| 2026-11-16 | Claude | Log why a remote actor could not be spawned |
| 2026-11-17 | Zeus Hernández | Fix a crash on loading a save with a reference left in a freed cell |

## Fixes in the bundled libraries (submodules)

| Library | Author | Change |
|---|---|---|
| TiltedConnect | obesecatlord | fix: enforce client owner and safe resolver teardown |
| TiltedReverse | obesecatlord | fix: report and own hook install failures |
| TiltedReverse | obesecatlord | fix: make hook teardown transactional |
| TiltedUI | jotagalindo | Draw the UI on the CPU so it keeps up when the game loads the GPU (ultrawide fix; the submodule pointer comes from his commit above) |

TiltedConnect and TiltedReverse point at `HAB3X/TiltedConnect` and `HAB3X/TiltedReverse`, branch `mac`.

## Left out because they did not build together with the rest

- 2026-10-29 Fallout Together Reborn Contributors: Add headless FalloutBot, client auto-connect and build fixes
- 2026-10-09 MostExcellent: Guarantee BSAnimationGraphManager ref-count Release with std::unique_ptr and deleter which releases the reference. This was not previously done on early returns in TESObjectREFR save/load variables, and never in DrawAnimDebugView.
- 2026-09-05 MrMartexX: Gate cell discovery on mod mapping readiness
- 2026-10-24 absol89: Review diagnostics for BSReadWriteLock stalls (revert before release)
- 2026-10-21 absol89: feat: redesign name tags and replace stranger names with Known names
- 2026-10-17 Daniil Zakharov: Run held member quest stages before a quest stops
- 2026-10-31 Claude: Fix stuck weapon states, door loops and lost gear on copies
- 2026-10-21 absol89: Fix crash when a remote actor is teleported into an unloaded exterior cell
