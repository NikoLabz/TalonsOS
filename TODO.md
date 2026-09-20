# TalonsOS Development Roadmap

> Long-term development roadmap for TalonsOS and the Talons ecosystem.
>
> Goal: Build a complete operating system and eventually a general-purpose,
> self-hosting programming language and development ecosystem.

---

# 🦅 PHASE 0 - PROJECT CREATION

* [x] Create the TalonsOS project
* [x] Establish the TalonsOS name
* [x] Establish the Talons/TalonsOS identity
* [x] Create initial project structure
* [x] Create initial Makefile
* [x] Establish x86/x86-64 target
* [x] Establish boot process
* [x] Create initial boot assembly
* [x] Create kernel entry assembly
* [x] Create initial C++ kernel
* [x] Create VGA output system
* [x] Create initial terminal system

---

# 🖥 PHASE 1 - BASIC KERNEL

* [x] Kernel successfully builds
* [x] Kernel binary generation
* [x] TalonsOS disk image generation
* [x] Boot TalonsOS in a virtual machine
* [x] VGA text output
* [x] Terminal cursor/output position tracking
* [x] Keyboard input
* [x] Basic command-line shell
* [x] Shell prompt
* [x] Command parsing
* [x] Unknown-command handling
* [x] Fix terminal output cursor persistence
* [x] Fix multiline shell output
* [x] Test filesystem command output

---

# ⌨ PHASE 2 - TERMINAL & SHELL

* [ ] Improve keyboard handling
* [ ] Backspace support *(basic backspace already works in terminal_run(); revisit for edge cases)*
* [ ] Arrow-key support
* [ ] Command history
* [ ] Better cursor movement
* [ ] Home/End support
* [ ] Delete key support
* [ ] Screen clearing
* [ ] Terminal scrolling *(scroll-on-overflow already implemented in vga_scroll(); revisit for a dedicated clear command)*
* [ ] Better command parsing
* [x] Command arguments *(command_matches()/copy_trimmed_argument() in shell.cpp already parse trailing arguments)*
* [ ] Command flags/options
* [ ] Built-in help command
* [ ] Built-in version command
* [ ] Built-in clear command
* [ ] Built-in reboot command
* [ ] Built-in shutdown command
* [ ] Shell error messages *(basic "X failed" messages exist per-command; revisit for consistency)*
* [ ] Shell command registry *(commands are currently a hardcoded if/else chain in shell_handle_line(); a real registry is still open)*
* [ ] Command aliases
* [ ] Tab completion
* [ ] Shell environment variables
* [ ] Command execution framework
* [ ] Standardised command return/error codes

---

# 💾 PHASE 3 - TALONS FILESYSTEM (TalonsFS)

## CORE

* [x] Filesystem layout
* [x] Superblock
* [x] Node system
* [x] Directory entries
* [x] Block allocation
* [x] Node allocation
* [x] Filesystem creation
* [x] Filesystem mounting

## FILE OPERATIONS

* [x] File creation
* [x] File reading
* [x] File writing
* [x] File deletion
* [x] File renaming/moving

## DIRECTORIES

* [x] Directory creation
* [x] Directory deletion
* [x] Directory navigation
* [x] Current working directory
* [x] Path handling

## INTEGRITY

* [x] Superblock checksum
* [x] Superblock corruption detection
* [x] Full metadata integrity checking
* [x] Bitmap consistency checking
* [x] Node consistency checking
* [x] Directory consistency checking
* [x] Filesystem corruption test suite

## REMAINING CORE FEATURES

* [x] File permissions
* [x] Timestamps
* [x] Filesystem unmounting
* [x] In-OS filesystem formatting
* [x] Basic filesystem recovery
* [ ] Structured filesystem error handling
* [ ] Filesystem caching
* [ ] Filesystem buffering

## DOCUMENTATION

* [ ] TalonsFS specification
* [ ] TalonsFS disk format documentation
* [ ] TalonsFS developer documentation

> **Current focus:** Filesystem integrity.
>
> The superblock checksum and corruption detection are complete. The next objective is to validate the consistency of the filesystem metadata, including the filesystem layout, bitmaps, nodes, and directory structures.
>
> TalonsFS should be considered complete for Phase 3 once its core operations, integrity checking, basic recovery, remaining filesystem features, testing, and documentation are complete.


---

# 📄 PHASE 4 - FILE HANDLING

* [ ] Implement basic text files
* [ ] Implement binary files
* [ ] Implement file handles
* [ ] Implement file descriptors
* [ ] Implement open()
* [ ] Implement close()
* [ ] Implement read()
* [ ] Implement write()
* [ ] Implement seek()
* [ ] Implement file size queries
* [ ] Implement directory handles
* [x] Implement filesystem API *(kernel/filesystem.h — name-based rather than descriptor-based; the open/close/read/write/seek items above describe a different, handle-based API still to build)*
* [ ] Implement `.tal` file recognition
* [ ] Implement `.tal` file loading
* [ ] Define standard file types
* [ ] Define standard paths
* [ ] Implement stdin
* [ ] Implement stdout
* [ ] Implement stderr
* [ ] Implement pipes
* [ ] Implement basic redirection

---

# 🧠 PHASE 5 - MEMORY MANAGEMENT

* [ ] Physical memory detection
* [ ] Memory map
* [ ] Physical memory manager
* [ ] Page/frame allocator
* [ ] Virtual memory
* [ ] Paging *(identity-mapped first 2MiB exists in entry.asm as part of the long-mode transition — full paging/virtual memory support is still open)*
* [x] Kernel heap *(memory.cpp: HEAP_START 0x100000, 1MiB heap, fixed 256-slot allocation table)*
* [x] Kernel allocator *(kmalloc()/kfree() in memory.cpp — see cleanup note below)*
* [ ] User-space memory
* [ ] Memory protection
* [ ] Memory debugging tools
* [ ] Out-of-memory handling *(kmalloc() returns nullptr on exhaustion; nothing currently checks/handles that at call sites)*
* [ ] Memory statistics
* [ ] Heap corruption detection
* [ ] Page fault handling

> **Cleanup note:** there were previously *two* competing kmalloc() implementations
> (`kernel/memory.cpp` and an orphaned `kernel/allocator.cpp`/`allocator.h`) which collide
> at link time since `size_t` and `uint64_t` mangle identically on x86-64. Only `memory.cpp`
> is actually wired into the kernel (`new_delete.cpp` uses it) — `allocator.cpp`/`.h` should
> be deleted. Also: `kernel_main()` never calls `memory_init()` — currently harmless because
> the heap's statics are zero by default, but worth adding explicitly.

---

# ⚙ PHASE 6 - CPU & HARDWARE

* [x] GDT *(boot.asm 32-bit GDT, entry.asm 64-bit GDT)*
* [ ] CPU detection
* [ ] CPU feature detection
* [ ] IDT *(no IDT is loaded anywhere yet — any CPU exception after entering long mode currently triple-faults with no handler)*
* [ ] Interrupt handling
* [ ] Exception handling
* [ ] Timer
* [ ] PIT/APIC support
* [ ] RTC
* [ ] PCI enumeration
* [ ] Hardware abstraction layer
* [ ] Device management
* [ ] CPU timekeeping
* [ ] Hardware error handling

---

# 🧵 PHASE 7 - PROCESSES & TASKS

* [ ] Kernel task structure
* [ ] Context switching
* [ ] Scheduler
* [ ] Process creation
* [ ] Process termination
* [ ] User mode
* [ ] System calls
* [ ] Process isolation
* [ ] Inter-process communication
* [ ] Signals/events
* [ ] Process management commands
* [ ] Program loading
* [ ] Executable format
* [ ] Process environment
* [ ] Process permissions
* [ ] Process resource limits

---

# 🖱 PHASE 8 - DRIVERS

* [x] Keyboard driver *(kernel/keyboard.cpp — polling PS/2, UK scancode set 1, shift handling)*
* [x] Storage driver *(kernel/block_device.cpp — ATA PIO read/write)*
* [ ] Keyboard driver improvements *(no interrupt-driven input yet — keyboard_poll_char() is polled from the terminal loop)*
* [ ] Mouse driver
* [ ] Display driver
* [ ] PCI drivers
* [ ] Network driver
* [ ] Audio driver
* [ ] USB support
* [ ] Driver framework
* [ ] Driver discovery
* [ ] Driver loading
* [ ] Driver unloading
* [ ] Device identification
* [ ] Device error handling

---

# 🌐 PHASE 9 - NETWORKING

* [ ] Network abstraction
* [ ] Ethernet
* [ ] ARP
* [ ] IPv4
* [ ] IPv6
* [ ] UDP
* [ ] TCP
* [ ] DNS
* [ ] Network configuration
* [ ] Network tools
* [ ] Basic sockets API
* [ ] Network interface management
* [ ] Loopback interface
* [ ] Routing
* [ ] Network diagnostics
* [ ] `ping` equivalent
* [ ] Network security foundations

---

# 🪟 PHASE 10 - GRAPHICS

* [ ] Graphics mode
* [ ] Framebuffer
* [ ] Basic drawing primitives
* [ ] Fonts
* [ ] Text rendering
* [ ] Window system
* [ ] Mouse cursor
* [ ] Windows
* [ ] Input events
* [ ] Basic GUI toolkit
* [ ] Desktop environment
* [ ] Display configuration
* [ ] Resolution management

---

# 🛠 PHASE 11 - SYSTEM UTILITIES

* [ ] File manager
* [ ] Text editor
* [ ] System monitor
* [ ] Process viewer
* [ ] Memory viewer
* [ ] Disk utilities
* [ ] Network utilities
* [ ] System information utility
* [ ] Configuration system
* [ ] Package/application system
* [ ] Logging system
* [ ] System diagnostics
* [ ] System recovery utilities
* [ ] Hardware information utility
* [ ] User management utilities

---

# 🔐 PHASE 12 - SECURITY & SYSTEM INTEGRITY

* [ ] User accounts
* [ ] Authentication
* [ ] Permission enforcement
* [ ] Privilege separation
* [ ] Secure system-call validation
* [ ] Executable validation
* [ ] Filesystem integrity verification
* [ ] System integrity verification
* [ ] Trusted boot components
* [ ] Security event logging
* [ ] Malware/compromise detection framework
* [ ] Recovery-state handling
* [ ] Secure recovery environment

---

# 🦅 PHASE 13 - TALONS SYSTEM RECOVERY & ATOMIC UPDATES

> TalonsOS uses three normal OS/generation slots and one dedicated recovery environment.

### System slots

```text
Left Claw   → normal OS / generation
Right Claw  → normal OS / generation
Left Wing   → normal OS / generation
Right Wing  → dedicated recovery OS
```

* [ ] Define Left Claw slot
* [ ] Define Right Claw slot
* [ ] Define Left Wing slot
* [ ] Define Right Wing recovery environment
* [ ] Define slot metadata
* [ ] Define slot states
* [ ] Define active slot
* [ ] Define inactive slots
* [ ] Define compromised/quarantined state
* [ ] Implement atomic system updates
* [ ] Build new generations in inactive slots
* [ ] Verify new generations before activation
* [ ] Implement generation switching
* [ ] Implement generation rollback
* [ ] Implement boot failure detection
* [ ] Implement automatic rollback
* [ ] Implement slot quarantine
* [ ] Implement recovery boot
* [ ] Implement Right Wing recovery tools
* [ ] Implement system rebuild
* [ ] Implement damaged-slot repair
* [ ] Implement generation restoration
* [ ] Implement recovery diagnostics
* [ ] Implement system integrity verification
* [ ] Implement recovery logging
* [ ] Implement safe slot deletion

### Explode / Kill emergency system

* [ ] Define `Explode`
* [ ] Define `Kill`
* [ ] Define catastrophic threat conditions
* [ ] Implement emergency system shutdown
* [ ] Mark compromised slot as quarantined
* [ ] Prevent compromised slot from normal boot
* [ ] Force Right Wing recovery
* [ ] Require compromised slot repair before normal recovery completion
* [ ] Preserve compromised slot for investigation/recovery
* [ ] Allow user to rebuild compromised slot
* [ ] Verify repaired slot
* [ ] Restore repaired slot to service

> `Explode` must not physically destroy hardware or blindly erase the entire disk. Its purpose is to isolate a catastrophically compromised TalonsOS installation and transfer control to the trusted Right Wing recovery environment.

---

# 🧠 PHASE 14 - TALONS LANGUAGE DESIGN

> Talons is intended to become a general-purpose programming language.
> TalonsOS is initially its primary environment.

## Language philosophy

* [x] General-purpose language
* [x] TalonsOS-first ecosystem
* [x] Eventually usable outside TalonsOS
* [x] Talons has its own syntax
* [x] Statically typed
* [x] Type inference
* [x] Hybrid memory management
* [ ] Finalise memory safety model
* [ ] Finalise ownership rules
* [ ] Finalise pointer/reference model

## Rough syntax prototype

```text
//// start.innit=True
if.start=True && innit !4
begin{!
    *say"hello! from Talons!"
?}
////
```

Current syntax concepts:

```text
//// marks the beginning/end of a Talons program
start.innit=True represents active program initialization
if.start=True validates the startup state
&& connects the startup condition
!4 is a deliberate structural/decorative syntax element
begin{! opens a block
*say"..." outputs text
?} closes a block
```

## Finalise syntax

* [ ] Finalise grammar
* [ ] Finalise keywords
* [ ] Finalise operators
* [ ] Finalise literals
* [ ] Finalise comments
* [ ] Finalise modules
* [ ] Finalise imports
* [ ] Finalise functions
* [ ] Finalise variables
* [ ] Finalise structs
* [ ] Finalise enums
* [ ] Finalise generics
* [ ] Finalise error handling
* [ ] Finalise concurrency
* [ ] Finalise standard library
* [ ] Finalise naming conventions
* [ ] Finalise ABI
* [ ] Finalise calling conventions
* [ ] Finalise package/module format
* [ ] Finalise interoperability rules

---

# 🔧 PHASE 15 - TALONS COMPILER

* [ ] Design compiler architecture
* [ ] Lexer
* [ ] Token definitions
* [ ] Parser
* [ ] AST
* [ ] Semantic analysis
* [ ] Type checker
* [ ] Type inference
* [ ] Error reporting
* [ ] Intermediate representation
* [ ] Optimisation pipeline
* [ ] Code generation
* [ ] Object file generation
* [ ] Executable generation
* [ ] TalonsOS executable format
* [ ] `.tal` source-file support
* [ ] Talons standard library
* [ ] Talons runtime
* [ ] Compiler command-line interface
* [ ] Compiler documentation
* [ ] Compiler test suite
* [ ] Compiler diagnostics
* [ ] Source locations
* [ ] Debug information
* [ ] Incremental compilation
* [ ] Cross-compilation

---

# 🔄 PHASE 16 - SELF-HOSTING TALONS

* [ ] Write basic Talons programs
* [ ] Port utilities to Talons
* [ ] Port TalonsOS libraries to Talons
* [ ] Port TalonsOS shell to Talons
* [ ] Port filesystem tools to Talons
* [ ] Implement compiler components in Talons
* [ ] Rewrite Talons compiler in Talons
* [ ] Compile Talons compiler using Talons
* [ ] Talons compiler successfully compiles itself
* [ ] Achieve self-hosting Talons

---

# 🖥 PHASE 17 - TALONSOS BUILT WITH TALONS

* [ ] Talons kernel components written in Talons
* [ ] Talons filesystem written in Talons
* [ ] Talons shell written in Talons
* [ ] Talons drivers written in Talons where practical
* [ ] TalonsOS utilities written in Talons
* [ ] TalonsOS standard libraries
* [ ] Minimise C/C++ dependencies
* [ ] Minimise assembly dependencies
* [ ] TalonsOS primarily built using Talons

---

# 📦 PHASE 18 - GENERAL-PURPOSE TALONS

* [ ] Talons compiler runs outside TalonsOS
* [ ] Linux support
* [ ] Windows support
* [ ] Other UNIX-like systems
* [ ] Cross-compilation
* [ ] Native executable targets
* [ ] Foreign-function interface
* [ ] External libraries
* [ ] Package manager
* [ ] Dependency management
* [ ] Developer tooling
* [ ] Language server
* [ ] Formatter
* [ ] Debugger
* [ ] Documentation generator

---

# 🌍 PHASE 19 - PUBLIC RELEASE

* [ ] Decide project licence
* [ ] Review third-party licences
* [ ] Establish contribution guidelines
* [ ] Establish code of conduct
* [ ] Establish versioning scheme
* [ ] Create public repository
* [ ] Write project documentation
* [ ] Write Talons language specification
* [ ] Write TalonsOS documentation
* [ ] Create installation instructions
* [ ] Create developer documentation
* [ ] Create examples
* [ ] Create tutorials
* [ ] Create release builds
* [ ] Create issue templates
* [ ] Create release process
* [ ] Review trademark/name considerations
* [ ] First public Talons release
* [ ] First public TalonsOS release

---

# 🏁 FINAL TALONSOS MILESTONE

The system has matured enough to have its own identity from the moment it boots.

Desired final boot sequence:

```text
TalonsOS System ready...


ready to boot?


Y/N?
```

On `Y`:

```text
Booting TalonsOS...
```

Then TalonsOS starts normally.

* [ ] Implement final boot prompt
* [ ] Implement Y/N input
* [ ] Implement boot confirmation
* [ ] Implement final polished startup sequence

---

# 🦅 LONG-TERM VISION

The ultimate goal is a coherent ecosystem:

```text
                 TALONS
                   │
          ┌────────┼────────┐
          │        │        │
       Compiler  Runtime  Libraries
          │
          ↓
       TalonsOS
          │
     ┌────┼────┐
     │    │    │
 Kernel  FS   Shell
     │
  Drivers
     │
 Hardware
```

Eventually:

```text
Talons compiler
      ↓
written in Talons
      ↓
compiles itself
      ↓
runs on TalonsOS
      ↓
TalonsOS is primarily written in Talons
```

Ultimate milestone:

> **Talons can build Talons, and TalonsOS can run Talons.**

---

# 🧪 DEVELOPMENT & TESTING

* [x] Establish working QEMU development workflow
* [x] Establish initial Git repository
* [x] Create initial known-good Git snapshot
* [x] Create TalonsFS test suite *(23 host-side test_*.cpp files — see Phase 3)*
* [ ] Establish automated build testing *(no Makefile was present in the last shared snapshot of the project — see note below)*
* [ ] Establish automated boot testing
* [ ] Establish kernel test framework
* [ ] Establish filesystem test framework *(unit-level tests exist per Phase 3; no integration/boot-level test harness yet)*
* [ ] Establish shell test framework
* [ ] Establish Talons compiler test framework
* [ ] Establish regression tests
* [ ] Test every major kernel subsystem independently
* [ ] Test filesystem corruption/recovery
* [ ] Test failed boot recovery
* [ ] Test atomic update rollback
* [ ] Test compromised-slot quarantine
* [ ] Test Right Wing recovery
* [ ] Maintain known-good release snapshots

---

# 📚 DOCUMENTATION

* [ ] TalonsOS architecture documentation
* [ ] Kernel architecture documentation
* [ ] TalonsFS specification
* [ ] TalonsFS disk format documentation
* [ ] System-call documentation
* [ ] Driver API documentation
* [ ] Shell command documentation
* [ ] Talons language specification
* [ ] Talons compiler specification
* [ ] Talons standard library documentation
* [ ] Recovery/atomic-update documentation
* [ ] Developer setup documentation
* [ ] Build system documentation
* [ ] Coding conventions
* [ ] Contribution documentation

---

# 📌 CURRENT STATUS

**Current active phase**

PHASE 3 - TalonsFS is largely functional (mount, create/read/write/delete files and
directories, navigation with cwd tracking, a 23-file unit test suite). Remaining open
items are permissions, timestamps, unmount, integrity checks, and in-OS formatting —
worth deciding whether those block moving on to Phase 2 (shell polish) or Phase 6 (IDT/
exception handling, currently entirely missing) in parallel.

**Last verified working state (build + boot audit)**

The kernel was rebuilt from source and booted successfully in QEMU end-to-end:
bootloader → protected mode → long mode → `kernel_main()` → TalonsFS mount → shell
prompt, confirmed by reading the VGA text buffer directly:

```text
TALONSOS KERNEL ONLINE.  TalonsFS: mounted
Developer@TalonsOS>
```

**Issues found and fixed during that audit**

* Duplicate `kmalloc()` between `memory.cpp` and orphaned `allocator.cpp`/`allocator.h`
  (collides at link time on x86-64 — delete the orphaned pair)
* `memory_init()` never called from `kernel_main()` (currently harmless, but fragile)
* A prior boot-hang traced to `-mcmodel=large` in the build flags — switched to
  `-mcmodel=small`, which is correct for a kernel loaded at a fixed low address (0x1000 at
  the time; now 0x10000)
* No IDT is loaded anywhere yet — any CPU exception after entering long mode currently
  triple-faults with no handler (tracked under Phase 6)
* No Makefile/linker script was present in the project snapshot used for this audit —
  reconstructed and verified working; confirm it matches your actual build setup

**Boot layout and loader (current)**

The kernel loads and links at `0x10000`. `boot/boot.asm` (`KERNEL_BASE`) and `linker.ld`
(`. = 0x10000`) must always agree; a mismatch triple-faults inside `entry.asm` because
every absolute address (GDT, far jump, vtables) is wrong. Older revisions used `0x0800`
(and briefly `0x0600`); those are obsolete.

```text
0x00000 - 0x004FF   IVT + BIOS data area
0x00500 - 0x07BFF   real-mode stack (grows down from 0x7C00)
0x07C00 - 0x07DFF   boot sector + 32-bit GDT
0x10000 - 0x1FDFF   kernel flat image (max 127 sectors)
0x17xxx - <=0x7FFFF kernel .bss (not stored on disk; zeroed by entry.asm)
0x90000 - 0x92FFF   page tables; stack grows down from 0x90000
0x100000 - 0x1FFFFF kernel heap (needs A20, enabled by boot.asm)

Disk: block 0 = bootloader, blocks 1-127 = kernel, block 128+ = TalonsFS
```

* Kernel is read with the INT 13h extended (LBA) read — no CHS geometry / track limits
* `boot.asm` fails the build if the kernel exceeds 127 sectors; `linker.ld` fails the
  build if kernel + `.bss` would reach `0x80000`
* `boot.asm` enables A20 (BIOS INT 15h, then port 0x92, then the 8042) and halts with
  `A20 FAILED.` if it cannot
* `entry.asm` zeroes `.bss` (`__bss_start` .. `__bss_end`) before any C++ runs

**Last confirmed working feature (pre-audit)**

```text
Developer@TalonsOS>LiterallyFDs!@Server


          @Server


[DIR]     ...
[DIR]     ...
[DIR]     ...


3 objects.
```

**Next task**

Given the audit findings, reasonable next steps in priority order:

* Delete the orphaned `allocator.cpp`/`allocator.h` and add the missing `memory_init()` call
* Decide on remaining Phase 3 items (permissions, timestamps, unmount) vs. moving to Phase 6
  (IDT/exception handling — notably absent, and the reason a bug anywhere past long-mode
  entry currently triple-faults instead of reporting an error)

**Current known-good Git snapshot**

```text
Commit: 9cfaf4f
Branch: main
Description: Initial TalonsOS snapshot
```

## Notes

This roadmap is intentionally long-term.

Do not attempt to complete multiple phases at once.

Build TalonsOS incrementally, test each component, and keep the system bootable whenever practical.

Talons language syntax is currently a prototype and may change before Talons Language v0.1 is finalised.

The Left Claw / Right Claw / Left Wing / Right Wing architecture is intentionally postponed until TalonsOS has a sufficiently mature system, filesystem, boot, integrity, and recovery foundation.

The Right Wing recovery environment is intended to remain a minimal, trusted environment rather than a normal OS generation.

Git provides development/source rollback, while TalonsOS atomic updates will eventually provide system-level generation rollback.
