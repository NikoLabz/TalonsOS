# TalonsOS Development Roadmap

> Long-term development roadmap for TalonsOS and the Talons ecosystem.
>
> Goal: Build a complete operating system and eventually a general-purpose, self-hosting programming language and development ecosystem.

## Progress markers

- [x] Complete and verified
- [ ] Not complete
- **In progress:** Partially implemented; requires further development and testing.
- **Blocked:** Cannot proceed until a dependency or design decision is resolved.

A feature is not complete merely because a source file exists or the project builds. Completion requires appropriate tests or demonstrated working behaviour.

---

# 🦅 PHASE 0 - PROJECT CREATION

- [x] Create the TalonsOS project
- [x] Establish the TalonsOS name
- [x] Establish the Talons/TalonsOS identity
- [x] Create initial project structure
- [x] Create initial Makefile
- [x] Establish x86/x86-64 target
- [x] Establish boot process
- [x] Create initial boot assembly
- [x] Create kernel entry assembly
- [x] Create initial C++ kernel
- [x] Create VGA output system
- [x] Create initial terminal system

---

# 🖥 PHASE 1 - BASIC KERNEL

- [x] Kernel successfully builds
- [x] Kernel binary generation
- [x] TalonsOS disk image generation
- [x] Boot TalonsOS in a virtual machine
- [x] VGA text output
- [x] Terminal cursor/output position tracking
- [x] Keyboard input
- [x] Basic command-line shell
- [x] Shell prompt
- [x] Command parsing
- [x] Unknown-command handling
- [x] Fix terminal output cursor persistence
- [x] Fix multiline shell output
- [x] Test filesystem command output

---

# ⌨ PHASE 2 - TERMINAL & SHELL

## KEYBOARD & INPUT

- [x] Basic keyboard handling
- [x] Backspace support
- [ ] Left/right arrow-key support
- [x] Up/down arrow-key support
- [ ] Better cursor movement
- [ ] Home/End support
- [ ] Delete key support
- [x] Input length protection
- [ ] Text insertion at cursor
- [ ] Text deletion at cursor
- [ ] Improved input editing

## COMMAND HISTORY

- [x] Command history
- [x] History navigation
- [x] History editing
- [x] Restore partially typed command after history navigation
- [ ] History improvements / edge cases

## TERMINAL

- [x] Cursor positioning
- [x] Hardware cursor updates
- [x] Screen clearing internally
- [x] Terminal scrolling
- [ ] Built-in clear command
- [ ] Improved terminal redraw behaviour

## COMMAND PARSING

- [x] Basic command parsing
- [x] Command arguments
- [ ] Command flags/options
- [ ] Quoted arguments
- [ ] Improved whitespace handling
- [ ] Command parsing errors

## BUILT-IN COMMANDS

- [ ] Built-in help command
- [ ] Built-in version command
- [ ] Built-in clear command
- [x] Built-in reboot command
- [ ] Built-in shutdown command
- [ ] More TalonsOS system commands

## SHELL ERRORS

- [x] Basic command failure messages
- [x] Structured filesystem error reporting
- [x] Unknown-command handling
- [ ] Standardised shell error messages
- [ ] Standardised command return/error codes

## COMMAND SYSTEM

- [ ] Shell command registry
- [ ] Command execution framework
- [ ] Command aliases
- [ ] Tab completion
- [ ] Command metadata/help descriptions

## SHELL ENVIRONMENT

- [ ] Shell environment variables
- [ ] Variable lookup
- [ ] Variable expansion
- [ ] Basic shell configuration

## PHASE 2 COMPLETION

- [ ] Complete terminal input editing
- [ ] Complete command parser
- [ ] Complete command registry
- [ ] Complete built-in command system
- [ ] Complete shell error handling
- [ ] Complete command completion
- [ ] Complete shell environment
- [ ] Full Phase 2 test pass

---

# 💾 PHASE 3 - TALONS FILESYSTEM (TalonsFS)

## CORE

- [x] Filesystem layout
- [x] Superblock
- [x] Node system
- [x] Directory entries
- [x] Block allocation
- [x] Node allocation
- [x] Filesystem creation
- [x] Filesystem mounting

## FILE OPERATIONS

- [x] File creation
- [x] File reading
- [x] File writing
- [x] File deletion
- [x] File renaming/moving

## DIRECTORIES

- [x] Directory creation
- [x] Directory deletion
- [x] Directory navigation
- [x] Current working directory
- [x] Path handling

## INTEGRITY

- [x] Superblock checksum
- [x] Superblock corruption detection
- [x] Full metadata integrity checking
- [x] Bitmap consistency checking
- [x] Node consistency checking
- [x] Directory consistency checking
- [x] Filesystem corruption test suite

## REMAINING CORE FEATURES

- [x] File permissions
- [x] Timestamps
- [x] Filesystem unmounting
- [x] In-OS filesystem formatting
- [x] Basic filesystem recovery
- [x] Structured filesystem error handling
- [x] Filesystem caching
- [x] Filesystem buffering

## DOCUMENTATION

- [x] TalonsFS specification
- [x] TalonsFS disk format documentation
- [x] TalonsFS developer documentation

> **Status:** The core filesystem and integrity features are marked complete based on the existing roadmap. Re-run the relevant test suite if changes affect TalonsFS.
>
> **Remaining work:** Confirm that permissions are enforced where intended, recovery behaves safely after actual corruption, and file operations remain reliable when used by the shell, editor, and compiler. These are verification and hardening tasks, not reasons to duplicate already completed checklist items.

---

# 📄 PHASE 4 - FILE HANDLING

- [x] Implement basic text files
- [ ] Implement binary files
- [x] Implement file handles
- [x] Implement file descriptors
- [x] Implement open()
- [x] Implement close()
- [x] Implement read()
- [x] Implement write()
- [x] Implement seek()
- [x] Implement file size queries
- [x] Implement directory handles
- [x] Implement filesystem API *(kernel/filesystem.h currently provides a name-based API; verify how much of the descriptor-based API is implemented independently)*
- [x] Implement `.tal` file recognition
- [x] Implement `.tal` file loading
- [x] Define standard file types
- [x] Define standard paths
- [x] Implement stdin
- [x] Implement stdout
- [x] Implement stderr
- [x] Implement pipes
- [x] Implement basic redirection

> **Note:** The checked descriptor and file-operation items describe functionality reported by the original roadmap. The name-based filesystem API is not automatically equivalent to a complete descriptor-based file API. Keep these items checked only while the corresponding implementation and tests remain valid.

---

# 🧠 PHASE 5 - MEMORY MANAGEMENT

- [ ] Physical memory detection
- [ ] Memory map
- [ ] Physical memory manager
- [ ] Page/frame allocator
- [ ] Virtual memory
- [ ] Paging *(identity-mapped first 2 MiB exists in entry.asm as part of the long-mode transition; full paging/virtual memory support remains open)*
- [x] Kernel heap *(memory.cpp: HEAP_START 0x100000, 1 MiB heap, fixed 256-slot allocation table)*
- [x] Kernel allocator *(kmalloc()/kfree() in memory.cpp; verify the current implementation before extending it)*
- [ ] User-space memory
- [ ] Memory protection
- [ ] Memory debugging tools
- [ ] Out-of-memory handling *(kmalloc() reportedly returns nullptr on exhaustion; call sites need auditing and reliable failure handling)*
- [ ] Memory statistics
- [ ] Heap corruption detection
- [ ] Page fault handling

> **Cleanup note:** The earlier roadmap identified competing `kmalloc()` implementations in `kernel/memory.cpp` and an orphaned `kernel/allocator.cpp` / `allocator.h`. Verify whether those files still exist and whether they are compiled before deleting anything.
>
> `kernel_main()` should explicitly initialise memory before any allocation is attempted. The current code calls `memory_init()`, but this must remain verified after kernel startup changes.

---

# ⚙ PHASE 6 - CPU & HARDWARE

- [x] GDT *(boot.asm 32-bit GDT, entry.asm 64-bit GDT)*
- [ ] CPU detection
- [ ] CPU feature detection
- [ ] IDT *(verify whether an IDT is loaded; the earlier audit found no IDT setup)*
- [ ] Interrupt handling
- [ ] Exception handling
- [ ] Timer
- [ ] PIT/APIC support
- [ ] RTC
- [ ] PCI enumeration
- [ ] Hardware abstraction layer
- [ ] Device management
- [x] CPU timekeeping *(kernel_time was previously included in the build; verify the actual capabilities before treating this as complete)*
- [ ] Hardware error handling

> **Priority note:** IDT and exception handling are important reliability gaps. Without valid exception handlers, certain CPU exceptions may lead to a triple fault and reset rather than a useful diagnostic. Revisit this phase after the initial compiler milestone or earlier if kernel stability becomes a blocker.

---

# 🧵 PHASE 7 - PROCESSES & TASKS

- [ ] Kernel task structure
- [ ] Context switching
- [ ] Scheduler
- [ ] Process creation
- [ ] Process termination
- [ ] User mode
- [ ] System calls
- [ ] Process isolation
- [ ] Inter-process communication
- [ ] Signals/events
- [ ] Process management commands
- [ ] Program loading
- [ ] Executable format
- [ ] Process environment
- [ ] Process permissions
- [ ] Process resource limits

> **Compiler dependency:** Native executable generation and execution will eventually need a defined executable format and a compatible loading mechanism. A compiler producing machine code does not, by itself, provide a safe process loader.

---

# 🖱 PHASE 8 - DRIVERS

- [x] Keyboard driver *(kernel/keyboard.cpp: polling PS/2, UK scancode set 1, shift handling)*
- [x] Storage driver *(kernel/block_device.cpp: ATA PIO read/write)*
- [ ] Keyboard driver improvements *(interrupt-driven input is not yet verified; keyboard polling is currently used by the terminal loop)*
- [ ] Mouse driver
- [ ] Display driver
- [ ] PCI drivers
- [ ] Network driver
- [ ] Audio driver
- [ ] USB support
- [ ] Driver framework
- [ ] Driver discovery
- [ ] Driver loading
- [ ] Driver unloading
- [ ] Device identification
- [ ] Device error handling

---

# 🌐 PHASE 9 - NETWORKING

- [ ] Network abstraction
- [ ] Ethernet
- [ ] ARP
- [ ] IPv4
- [ ] IPv6
- [ ] UDP
- [ ] TCP
- [ ] DNS
- [ ] Network configuration
- [ ] Network tools
- [ ] Basic sockets API
- [ ] Network interface management
- [ ] Loopback interface
- [ ] Routing
- [ ] Network diagnostics
- [ ] `ping` equivalent
- [ ] Network security foundations

---

# 🪟 PHASE 10 - GRAPHICS

- [ ] Graphics mode
- [ ] Framebuffer
- [ ] Basic drawing primitives
- [ ] Fonts
- [ ] Text rendering
- [ ] Window system
- [ ] Mouse cursor
- [ ] Windows
- [ ] Input events
- [ ] Basic GUI toolkit
- [ ] Desktop environment
- [ ] Display configuration
- [ ] Resolution management

---

# 🛠 PHASE 11 - SYSTEM UTILITIES

- [ ] File manager
- [x] Text editor *(the `talonedit` command has been integrated; verify the editor's current capabilities and save behaviour)*
- [ ] System monitor
- [ ] Process viewer
- [ ] Memory viewer
- [ ] Disk utilities
- [ ] Network utilities
- [ ] System information utility
- [ ] Configuration system
- [ ] Package/application system
- [ ] Logging system
- [ ] System diagnostics
- [ ] System recovery utilities
- [ ] Hardware information utility
- [ ] User management utilities

---

# 🔐 PHASE 12 - SECURITY & SYSTEM INTEGRITY

- [ ] User accounts
- [ ] Authentication
- [ ] Permission enforcement
- [ ] Privilege separation
- [ ] Secure system-call validation
- [ ] Executable validation
- [ ] Filesystem integrity verification
- [ ] System integrity verification
- [ ] Trusted boot components
- [ ] Malware/compromise detection framework
- [ ] Recovery-state handling
- [ ] Secure recovery environment

---

# 🦅 PHASE 13 - TALONS SYSTEM RECOVERY & ATOMIC UPDATES

> TalonsOS uses three normal OS/generation slots and one dedicated recovery environment.

## SYSTEM SLOTS

```text
Left Claw   → normal OS / generation
Right Claw  → normal OS / generation
Left Wing   → normal OS / generation
Right Wing  → dedicated recovery OS
```

- [ ] Define Left Claw slot
- [ ] Define Right Claw slot
- [ ] Define Left Wing slot
- [ ] Define Right Wing recovery environment
- [ ] Define slot metadata
- [ ] Define slot states
- [ ] Define active slot
- [ ] Define inactive slots
- [ ] Define compromised/quarantined state
- [ ] Implement atomic system updates
- [ ] Build new generations in inactive slots
- [ ] Verify new generations before activation
- [ ] Implement generation switching
- [ ] Implement generation rollback
- [ ] Implement boot failure detection
- [ ] Implement automatic rollback
- [ ] Implement slot quarantine
- [ ] Implement recovery boot
- [ ] Implement Right Wing recovery tools
- [ ] Implement system rebuild
- [ ] Implement damaged-slot repair
- [ ] Implement generation restoration
- [ ] Implement recovery diagnostics
- [ ] Implement system integrity verification
- [ ] Implement recovery logging
- [ ] Implement safe slot deletion

## EXPLODE / KILL EMERGENCY SYSTEM

- [ ] Define `Explode`
- [ ] Define `Kill`
- [ ] Define catastrophic threat conditions
- [ ] Implement emergency system shutdown
- [ ] Mark compromised slot as quarantined
- [ ] Prevent compromised slot from normal boot
- [ ] Force Right Wing recovery
- [ ] Require compromised slot repair before normal recovery completion
- [ ] Preserve compromised slot for investigation/recovery
- [ ] Allow user to rebuild compromised slot
- [ ] Verify repaired slot
- [ ] Restore repaired slot to service

> `Explode` must not physically destroy hardware or blindly erase the entire disk. Its purpose is to isolate a catastrophically compromised TalonsOS installation and transfer control to the trusted Right Wing recovery environment.

---

# 🧠 PHASE 14 - TALONS LANGUAGE DESIGN

> Talons is intended to become a general-purpose programming language.
>
> TalonsOS is initially its primary environment. The syntax below is a prototype and is not yet a final language specification.

## LANGUAGE PHILOSOPHY

- [x] General-purpose language *(design goal established)*
- [x] TalonsOS-first ecosystem
- [x] Eventually usable outside TalonsOS
- [x] Talons has its own syntax
- [x] Statically typed *(design goal; compiler enforcement still to implement)*
- [x] Type inference *(design goal; compiler implementation still to implement)*
- [x] Hybrid memory management *(design goal; final semantics remain open)*
- [ ] Finalise memory safety model
- [ ] Finalise ownership rules
- [ ] Finalise pointer/reference model

## ROUGH SYNTAX PROTOTYPE

```text
#%OS
////
start.innit=True
if.start=True && innit !4

begin{!
    *say"hello! from Talons!";
?}
////
```

Current syntax concepts:

```text
//// marks the beginning/end of a Talons program
#%OS identifies the intended TalonsOS source environment
start.innit=True represents active program initialization
if.start=True validates the startup state
&& connects the startup condition
!4 is a deliberate structural/decorative syntax element
begin{! opens a block
*say"..." outputs text
?} closes a block
```

> **Important:** The grammar and meaning of these constructs are still provisional. Their syntax, semantics, and error cases must be finalised before the compiler can be considered a stable language implementation.

## FINALISE SYNTAX

- [ ] Finalise grammar
- [ ] Finalise keywords
- [ ] Finalise operators
- [ ] Finalise literals
- [ ] Finalise comments
- [ ] Finalise modules
- [ ] Finalise imports
- [ ] Finalise functions
- [ ] Finalise variables
- [ ] Finalise structs
- [ ] Finalise enums
- [ ] Finalise generics
- [ ] Finalise error handling
- [ ] Finalise concurrency
- [ ] Finalise standard library
- [ ] Finalise naming conventions
- [ ] Finalise ABI
- [ ] Finalise calling conventions
- [ ] Finalise package/module format
- [ ] Finalise interoperability rules

---

# 🔧 PHASE 15 - TALONS COMPILER

> **CURRENT ACTIVE PHASE**
>
> Build a complete TalonC compiler before attempting self-hosting. Keep the compiler project independently buildable and testable on the development host while integrating it with TalonsOS as the target environment.

## ARCHITECTURE AND FRONTEND

- [ ] Design compiler architecture
- [ ] Define compiler pipeline and component interfaces
- [ ] Lexer *(initial implementation exists; review and expand)*
- [ ] Token definitions *(initial definitions exist; finalise against the grammar)*
- [ ] Parser *(initial implementation exists; review and expand)*
- [ ] Abstract syntax tree (AST)
- [ ] Finalise grammar for the first supported language version
- [ ] Implement source locations for tokens and syntax nodes
- [ ] Implement useful syntax-error reporting
- [ ] Reject invalid or unknown characters instead of silently ignoring them
- [ ] Add lexer tests
- [ ] Add parser tests
- [ ] Add malformed-source and edge-case tests

## SEMANTIC ANALYSIS

- [ ] Semantic analysis
- [ ] Symbol tables
- [ ] Variable declarations and scope
- [ ] Type checker
- [ ] Type inference
- [ ] Function and parameter checking
- [ ] Return-type checking
- [ ] Struct and enum checking
- [ ] Operator type checking
- [ ] Undefined-name detection
- [ ] Duplicate-declaration detection
- [ ] Useful semantic-error reporting
- [ ] Semantic-analysis test suite

## INTERMEDIATE REPRESENTATION

- [ ] Design TalonsIR
- [ ] Define TalonsIR instructions and data types
- [ ] Convert AST to TalonsIR
- [ ] Validate TalonsIR
- [ ] Define control-flow representation
- [ ] Define function and calling-convention representation
- [ ] Add TalonsIR tests
- [ ] Document TalonsIR

## OPTIMISATION

- [ ] Optimisation pipeline
- [ ] Constant folding
- [ ] Dead-code elimination
- [ ] Basic control-flow simplification
- [ ] Register-allocation preparation
- [ ] Optimisation correctness tests

## CODE GENERATION AND EXECUTABLES

- [ ] Code generation
- [ ] Define initial target architecture and platform ABI
- [ ] Native x86-64 code generation
- [ ] Register allocation
- [ ] Stack-frame generation
- [ ] Function calls and returns
- [ ] Branches and control flow
- [ ] Object file generation
- [ ] Executable generation
- [ ] TalonsOS executable format
- [ ] Executable validation
- [ ] Program loading integration
- [ ] Define how generated programs access system services
- [ ] Add code-generation tests
- [ ] Execute generated programs in a controlled test environment

## LANGUAGE SUPPORT AND RUNTIME

- [ ] `.tal` source-file support
- [ ] Variables and expressions
- [ ] Conditional statements
- [ ] Loops
- [ ] Functions
- [ ] Structs
- [ ] Enums
- [ ] Modules and imports
- [ ] Talons standard library
- [ ] Talons runtime
- [ ] Define memory-management behaviour
- [ ] Define error and panic behaviour
- [ ] Define system-call or runtime interfaces
- [ ] Add runtime tests

## COMMAND-LINE INTERFACE

- [ ] Compiler command-line interface
- [ ] Source-file input
- [ ] Output-file selection
- [ ] Syntax-check mode
- [ ] Diagnostics and error exit codes
- [ ] Compilation target selection
- [ ] Build options and flags
- [ ] Clear usage/help output

## TESTING AND DOCUMENTATION

- [ ] Compiler test suite
- [ ] Compiler diagnostics
- [ ] Source locations
- [ ] Debug information
- [ ] Incremental compilation
- [ ] Cross-compilation
- [ ] Regression tests
- [ ] Test invalid and malformed programs
- [ ] Test generated executable behaviour
- [ ] Compiler architecture documentation
- [ ] Compiler documentation
- [ ] Talons language specification
- [ ] Reproducible compiler builds

## TALONSC INTEGRATION WITH TALONSOS

- [x] Integrate the prototype `talonc` command into the TalonsOS kernel and shell
- [x] Load and execute a basic supported `.tal` test program through the prototype runner
- [x] Add compiler status output to the TalonsOS startup display *(verify final placement and status semantics)*
- [ ] Replace or extend the prototype runner with the actual compiler pipeline
- [ ] Validate complete source programs before execution
- [ ] Define how TalonsOS stores compiler outputs
- [ ] Integrate generated executable files with the OS program loader
- [ ] Add meaningful compiler failure reporting inside TalonsOS
- [ ] Test compilation and execution from the TalonsOS shell
- [ ] Verify that compiler errors do not corrupt terminal state
- [ ] Maintain host-side compiler tests independently of the OS build

> **Current implementation note:** The existing TalonC project contains an initial lexer, token definitions, parser, Makefile, and basic tests. The in-kernel `talonc` implementation is currently a small prototype runner that supports a limited subset of the syntax. These components are starting points, not evidence that the full compiler is complete.
>
> **Scope note:** Self-hosting is explicitly deferred. TalonC should first become a functional, tested compiler with a documented language subset and a reliable executable-generation path.

---

# 🔄 PHASE 16 - SELF-HOSTING TALONS

> Deferred until Phase 15 has produced a stable and sufficiently capable compiler.

- [ ] Write basic Talons programs
- [ ] Port utilities to Talons
- [ ] Port TalonsOS libraries to Talons
- [ ] Port TalonsOS shell to Talons
- [ ] Port filesystem tools to Talons
- [ ] Implement compiler components in Talons
- [ ] Rewrite Talons compiler in Talons
- [ ] Compile Talons compiler using Talons
- [ ] Talons compiler successfully compiles itself
- [ ] Achieve self-hosting Talons

---

# 🖥 PHASE 17 - TALONSOS BUILT WITH TALONS

> Deferred until the language, compiler, runtime, and system interfaces are sufficiently mature.

- [ ] Talons kernel components written in Talons
- [ ] Talons filesystem written in Talons
- [ ] Talons shell written in Talons
- [ ] Talons drivers written in Talons where practical
- [ ] TalonsOS utilities written in Talons
- [ ] TalonsOS standard libraries
- [ ] Minimise C/C++ dependencies
- [ ] Minimise assembly dependencies
- [ ] TalonsOS primarily built using Talons

---

# 📦 PHASE 18 - GENERAL-PURPOSE TALONS

- [ ] Talons compiler runs outside TalonsOS
- [ ] Linux support
- [ ] Windows support
- [ ] Other UNIX-like systems
- [ ] Cross-compilation
- [ ] Native executable targets
- [ ] Foreign-function interface
- [ ] External libraries
- [ ] Package manager
- [ ] Dependency management
- [ ] Developer tooling
- [ ] Language server
- [ ] Formatter
- [ ] Debugger
- [ ] Documentation generator

---

# 🌍 PHASE 19 - PUBLIC RELEASE

- [ ] Decide project licence
- [ ] Review third-party licences
- [ ] Establish contribution guidelines
- [ ] Establish code of conduct
- [ ] Establish versioning scheme
- [ ] Create public repository
- [ ] Write project documentation
- [ ] Write Talons language specification
- [ ] Write TalonsOS documentation
- [ ] Create installation instructions
- [ ] Create developer documentation
- [ ] Create examples
- [ ] Create tutorials
- [ ] Create release builds
- [ ] Create issue templates
- [ ] Create release process
- [ ] Review trademark/name considerations
- [ ] First public Talons release
- [ ] First public TalonsOS release

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

- [ ] Implement final boot prompt
- [ ] Implement Y/N input
- [ ] Implement boot confirmation
- [ ] Implement final polished startup sequence

---

# 🧪 DEVELOPMENT & TESTING

- [x] Establish working QEMU development workflow
- [x] Establish initial Git repository
- [x] Create initial known-good Git snapshot
- [x] Create TalonsFS test suite *(the existing roadmap records 23 host-side `test_*.cpp` files; verify the current count and test results)*
- [ ] Establish automated build testing
- [ ] Establish automated boot testing
- [ ] Establish kernel test framework
- [ ] Establish filesystem test framework *(unit-level tests exist according to the roadmap; integration and boot-level coverage remain to be verified)*
- [ ] Establish shell test framework
- [ ] Establish Talons compiler test framework
- [ ] Establish regression tests
- [ ] Test every major kernel subsystem independently
- [ ] Test filesystem corruption/recovery
- [ ] Test failed boot recovery
- [ ] Test atomic update rollback
- [ ] Test compromised-slot quarantine
- [ ] Test Right Wing recovery
- [ ] Maintain known-good release snapshots

> **Development rule:** Keep TalonsOS bootable whenever practical. Test individual compiler components before integrating them into the kernel. Keep known-good commits and avoid bundling unrelated changes into one commit.
>
> **Current compiler milestone:** Establish repeatable host-side TalonC builds and tests first, then test the integration in QEMU. A successful OS boot does not prove that the compiler generates correct machine code.

---

# 📚 DOCUMENTATION

- [ ] TalonsOS architecture documentation
- [ ] Kernel architecture documentation
- [x] TalonsFS specification
- [x] TalonsFS disk format documentation
- [ ] System-call documentation
- [ ] Driver API documentation
- [ ] Shell command documentation
- [ ] Talons language specification
- [ ] Talons compiler specification
- [ ] Talons standard library documentation
- [ ] Recovery/atomic-update documentation
- [ ] Developer setup documentation
- [ ] Build system documentation
- [ ] Coding conventions
- [ ] Contribution documentation

---

# 📌 CURRENT STATUS

**Current active phase**

PHASE 15 - TALONS COMPILER

The immediate objective is to build a full TalonC compiler before attempting self-hosting. The compiler should first be developed and tested as an independent project, then integrated with TalonsOS as its target environment.

**TalonsOS status**

The project has previously built and booted in QEMU, mounted TalonsFS, and provided a working shell. Core filesystem features and several terminal, file-handling, and memory components are already present.

The following have been demonstrated in the recent development session:

- [x] TalonsOS builds and boots in QEMU
- [x] TalonsFS mounts during startup
- [x] TalonsOS shell recognises the `talonc` command
- [x] The prototype runner loads a basic `.tal` file from the TalonsOS filesystem
- [x] The prototype runner executes a supported `*say` statement
- [x] A basic test prints `Hello from Talons!`
- [x] The TalonC integration was committed to Git

**Last recorded TalonC integration commit**

```text
Commit: c9a5106
Message: Integrate TalonC runner into TalonsOS
```

The working tree still had modified and untracked files after this commit. Inspect these before deciding whether they should be committed, retained, or removed.

**Known development caveats**

- The existing TalonC runner is a prototype, not a complete compiler.
- The initial lexer/parser and test suite need to be audited against the final grammar.
- The startup status message must accurately reflect the state of the TalonC subsystem.
- The boot layout, linker settings, memory layout, and filesystem behaviour should be verified against the current source rather than relying on historical audit notes.
- Self-hosting is not part of the immediate task.

**Next task**

Audit the existing TalonC project and test suite, then implement the compiler in stages:

1. Lexer and parser
2. AST and source diagnostics
3. Semantic analysis and type checking
4. TalonsIR
5. Optimisation
6. Native x86-64 code generation and executable generation
7. Runtime, standard library, and command-line interface
8. Integration and regression testing

Do not mark components complete until they are implemented and verified.

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

Self-hosting remains a long-term goal and will not be attempted until the compiler has a stable grammar, reliable semantic analysis, working code generation, and a tested runtime.

---

# 📝 ROADMAP MAINTENANCE NOTES

- Update this document when a feature is genuinely implemented or its status changes.
- Remove obsolete status notes when the corresponding issues have been resolved and verified.
- Keep historical technical findings in architecture documentation or Git history rather than allowing old audit notes to contradict the current checklist.
- Distinguish between a design goal, a prototype, a partial implementation, and a verified feature.
- Do not attempt to complete multiple major phases at once.
- Preserve the existing OS integration while developing the compiler independently.
- Defer Phase 16 and Phase 17 until Phase 15 is sufficiently complete.
- Keep the long-term vision intact without letting it distract from the current compiler milestone.
