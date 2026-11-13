#pragma once

// Shell reports for the physical memory map (Phase 5, M1).

// "meminfo": totals from the E820 map, frame allocator state, kernel end.
void meminfo_run(int stdoutFd, int stderrFd);

// "memmap": one line per E820 entry.
void memmap_run(int stdoutFd, int stderrFd);

// Run PMM and heap integrity checks and write their result directly to VGA.
// This path uses fixed stack storage and does not pass through file streams.
void memcheck_run();
