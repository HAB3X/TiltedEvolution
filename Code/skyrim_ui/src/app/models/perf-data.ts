/** The performance overlay's numbers for the last second. */
export interface PerfData {
  /** -1 for fps, frameMs and modMs while the game's frame profiler is off ([Debug] bFrameProfiler=false). */
  fps: number;
  frameMs: number;
  /** Skyrim Together's own service updates, milliseconds per frame. */
  modMs: number;
  ping: number;
  /** Actors and objects this game syncs. */
  actors: number;
}
