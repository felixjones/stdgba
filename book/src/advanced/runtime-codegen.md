# Runtime ARM Codegen

`<gba/codegen>` provides an ARM macro builder for compile-time code generation, with runtime copy/install and execution.

## Design goals

- builder for common ARM encodings
- compile-time diagnostics for invalid encodings
- automatic capacity inference for convenience

## API overview

| API | Purpose |
|-----|---------|
| `gba::codegen::arm_macro_builder<N>` | Instruction builder with checked encoders |
| `gba::codegen::arm_macro(...)` | Consteval helper that infers instruction capacity |
| `compiled_block<N>` | Compiled instruction block with `data()`, `size()`, and `size_bytes()` |
| `apply_patches<Sig>(...)` | Apply patch-slot arguments to installed code and return a raw typed function pointer |

## Builder usage

```cpp
#include <gba/codegen>

using namespace gba::codegen;

constexpr auto block = [] {
    auto b = arm_macro_builder<4>{};
    b.mov_imm(arm_reg::r0, 42)
     .bx(arm_reg::lr);
    return b.compile();
}();
```

When the block is compile-time generated, capacity can be inferred:

```cpp
#include <gba/codegen>

using namespace gba::codegen;

constexpr auto inferred = arm_macro([](auto& b) {
    b.mov_imm(arm_reg::r0, 7)
     .bx(arm_reg::lr);
});
```

`arm_macro(...)` automatically infers capacity by counting instructions during compilation.

Core checked encoders include:

- `mov_imm(rd, imm8)`
- `mov_reg(rd, rm)`
- `add_imm(rd, rn, imm8)`
- `add_reg(rd, rn, rm)`
- `sub_imm(rd, rn, imm8)`
- `sub_reg(rd, rn, rm)`
- `ldr_imm(rd, rn, signed12)`
- `str_imm(rd, rn, signed12)`
- `bx(rm)`
- `b_to(target_index)` (relative branch by emitted-word index)

## AAPCS runtime argument support

Generated leaf functions can consume call-time arguments using the standard ARM AAPCS register subset used on GBA:

- argument 0 -> `r0`
- argument 1 -> `r1`
- argument 2 -> `r2`
- argument 3 -> `r3`
- integer / pointer return -> `r0`

This works naturally through direct function-pointer casts and `apply_patches<Sig>(...)` when the generated instruction stream uses register-form operations.

Example: `int(int, int)` add function:

```cpp
constexpr auto add_args = arm_macro([](auto& b) {
    b.add_reg(arm_reg::r0, arm_reg::r0, arm_reg::r1)
     .bx(arm_reg::lr);
});

alignas(4) unsigned int code[4] = {};
std::memcpy(code, add_args.data(), add_args.size_bytes());
auto fn = reinterpret_cast<int (*)(int, int)>(code);

int result = fn(30, 12); // 42
```

Example: mix a runtime argument with a patch-time constant:

```cpp
constexpr auto scale_add = arm_macro([](auto& b) {
    b.add_reg(arm_reg::r0, arm_reg::r0, arm_reg::r0)
     .add_reg(arm_reg::r0, arm_reg::r0, arm_reg::r0)
     .add_imm(arm_reg::r0, arm_reg::r0, imm_slot(0))
     .bx(arm_reg::lr);
});

alignas(4) unsigned int code[8] = {};
std::memcpy(code, scale_add.data(), scale_add.size_bytes());
auto fn = apply_patches<int(int)>(scale_add, code, 8, 2u);

int result = fn(10); // 42 == 4 * 10 + 2
```

Checked full-instruction helpers for runtime whole-word patching:

- `nop_instr()`
- `add_reg_instr(rd, rn, rm)`
- `sub_reg_instr(rd, rn, rm)`

Patch-slot placeholders (v1.1):

- `imm_slot(n)` for `mov_imm`, `add_imm`, `sub_imm`
- `s12_slot(n)` for `ldr_imm`, `str_imm`
- `b_slot(n)` for `b_to` (branch offset in words)
- `instr_slot(n)` for `instruction(...)` (replace an entire 32-bit instruction word)

```cpp
constexpr auto tpl = arm_macro([](auto& b) {
    b.mov_imm(arm_reg::r0, imm_slot(0))
     .ldr_imm(arm_reg::r1, arm_reg::r2, s12_slot(1))
     .instruction(instr_slot(2))
     .b_to(b_slot(3))
     .bx(arm_reg::lr);
});

alignas(4) unsigned int code[8] = {};
std::memcpy(code, tpl.data(), tpl.size_bytes());

auto fn = apply_patches<void()>(tpl, code, 8,
                                5u,
                                -16,
                                add_reg_instr(arm_reg::r1, arm_reg::r1, arm_reg::r0),
                                -2);
```

## Runtime copy/install and patching

The runtime workflow is split into two stages to enable flexible re-patching:

**Stage 1: Copy bytes into executable RAM**

Copy the compiled instruction block into executable RAM with your preferred mechanism:

```cpp
#include <gba/codegen>

using namespace gba::codegen;

alignas(4) unsigned int code[8] = {};
std::memcpy(code, block.data(), block.size_bytes());

// Or use DMA / a custom copy routine if preferred.
```

**Stage 2: Patch and get a typed function pointer** (`apply_patches`)

Apply patch-slot arguments to already-installed code. This enables re-patching the same code with different arguments:

```cpp
// Patch with first set of arguments
auto fn1 = apply_patches<void()>(block, code, 8, 5u, -16, 9u);

// Re-patch with different arguments (overwrites previous patches)
auto fn2 = apply_patches<void()>(block, code, 8, 10u, 32, 12u);
```

## Safety notes

- Ensure the destination memory region is executable RAM.
- Invalid checked operands trigger compile-time diagnostics in constexpr/consteval contexts.
- Branch offsets are in 32-bit words and must fit the ARM B instruction range: [-2^23, 2^23).
- Whole-instruction patch slots write a full 32-bit instruction word; prefer checked helpers such as `nop_instr()`, `add_reg_instr(...)`, and `sub_reg_instr(...)` when selecting among runtime instruction variants.
- Current AAPCS support is intentionally small-scope: leaf-style integer/pointer arguments in `r0`..`r3` and integer/pointer return in `r0`. Stack arguments, callee-save register management, and calls out to other functions are not abstracted yet.

## Execution example

You can call generated code either by:

- copying bytes and using a direct `reinterpret_cast` to a function pointer, or
- using `apply_patches<Sig>(...)`, the single runtime patching API, which returns a raw typed function pointer after patching.

```cpp
#include <gba/codegen>

using namespace gba::codegen;

// Template that returns the sum of two immediates
constexpr auto add_template = arm_macro([](auto& b) {
    b.mov_imm(arm_reg::r0, imm_slot(0))
     .add_imm(arm_reg::r0, arm_reg::r0, imm_slot(1))
     .bx(arm_reg::lr);
});

alignas(4) unsigned int code[8] = {};

std::memcpy(code, add_template.data(), add_template.size_bytes());

// Generate and call with arguments (30 + 12 = 42)
auto fn = apply_patches<int()>(add_template, code, 8, 30u, 12u);
int result = fn();  // result is 42

// Re-patch and call with different arguments (100 + 200 = 300)
auto fn2 = apply_patches<int()>(add_template, code, 8, 100u, 200u);
int result2 = fn2();  // result2 is 300
```

## Whole-instruction patch example

You can reserve an entire instruction word and patch it at runtime:

```cpp
constexpr auto op_template = arm_macro([](auto& b) {
    b.mov_imm(arm_reg::r0, imm_slot(0))
     .mov_imm(arm_reg::r1, imm_slot(1))
     .instruction(instr_slot(2))
     .bx(arm_reg::lr);
});

alignas(4) unsigned int code[8] = {};
std::memcpy(code, op_template.data(), op_template.size_bytes());

// Replace the reserved word with an ADD instruction: r0 = r0 + r1
auto add_fn = apply_patches<int()>(op_template, code, 8,
                                   10u,
                                   3u,
                                   add_reg_instr(arm_reg::r0, arm_reg::r0, arm_reg::r1));
int added = add_fn();  // 13

// Re-patch the same instruction word to SUB: r0 = r0 - r1
auto sub_fn = apply_patches<int()>(op_template, code, 8,
                                   10u,
                                   3u,
                                   sub_reg_instr(arm_reg::r0, arm_reg::r0, arm_reg::r1));
int subtracted = sub_fn();  // 7
```

The generated code executes directly on the CPU with no overhead beyond the ARM instruction sequence you defined.

## Complex codegen examples

The instruction set includes bitwise operations, comparisons, conditional branches, multiply, and multi-register operations. Below are real-world examples that showcase these capabilities in sophisticated runtime code patterns.

### Callee-save register pattern with push/pop

A function that preserves callee-save registers across a computation:

```cpp
#include <gba/codegen>

using namespace gba::codegen;

// int complex_calc(int a, int b) - preserves r4-r6 across work
constexpr auto with_callee_save = arm_macro([](auto& b) {
    // Save callee-save registers
    b.push(0x0070);  // push {r4, r5, r6}
    
    // Use saved registers for temporaries
    b.mov_reg(arm_reg::r4, arm_reg::r0);  // save original a
    b.mov_reg(arm_reg::r5, arm_reg::r1);  // save original b
    
    // Complex computation: ((a * 3) + (b * 5)) & 0xFF
    b.add_reg(arm_reg::r6, arm_reg::r4, arm_reg::r4); // r6 = a * 2
    b.add_reg(arm_reg::r6, arm_reg::r6, arm_reg::r4); // r6 = a * 3
    
    b.add_reg(arm_reg::r0, arm_reg::r5, arm_reg::r5); // r0 = b * 2
    b.add_reg(arm_reg::r0, arm_reg::r0, arm_reg::r0); // r0 = b * 4
    b.add_reg(arm_reg::r0, arm_reg::r0, arm_reg::r5); // r0 = b * 5
    
    b.add_reg(arm_reg::r0, arm_reg::r6, arm_reg::r0); // sum
    b.and_imm(arm_reg::r0, arm_reg::r0, 0xFF);        // mask
    
    // Restore callee-save registers
    b.pop(0x0070);  // pop {r4, r5, r6}
    b.bx(arm_reg::lr);
});

alignas(4) unsigned int code[24] = {};
std::memcpy(code, with_callee_save.data(), with_callee_save.size_bytes());
auto fn = reinterpret_cast<int (*)(int, int)>(code);

int result = fn(10, 20); // (10*3 + 20*5) & 0xFF = 130
```

### Halfword sprite OAM update helper

Process halfword OAM attribute writes for sprite positioning:

```cpp
#include <gba/codegen>

using namespace gba::codegen;

// Write sprite position to OAM (halfword operations)
// void set_sprite_pos(u16* oam, int x, int y)
constexpr auto set_sprite_pos = arm_macro([](auto& b) {
    // r0 = OAM base
    // r1 = x coordinate
    // r2 = y coordinate
    
    // Load current attr0, clear Y bits, set new Y
    b.ldrh_imm(arm_reg::r3, arm_reg::r0, 0);      // load attr0
    b.and_imm(arm_reg::r3, arm_reg::r3, 0xFF);    // clear Y field
    b.orr_reg(arm_reg::r3, arm_reg::r3, arm_reg::r2); // set new Y
    b.strh_imm(arm_reg::r3, arm_reg::r0, 0);      // write attr0
    
    // Load current attr1, clear X bits, set new X
    b.ldrh_imm(arm_reg::r3, arm_reg::r0, 2);      // load attr1
    b.bic_imm(arm_reg::r3, arm_reg::r3, 0xFF);    // clear low X bits
    b.orr_reg(arm_reg::r3, arm_reg::r3, arm_reg::r1); // set new X
    b.strh_imm(arm_reg::r3, arm_reg::r0, 2);      // write attr1
    
    b.bx(arm_reg::lr);
});
```

### Fast multiply-accumulate with shift

Compute `(a * b) + (c << shift)` using multiply and shift-by-register:

```cpp
#include <gba/codegen>

using namespace gba::codegen;

// int mul_add_shift(int a, int b, int c, int shift)
// returns (a * b) + (c << shift)
constexpr auto mul_add_shift = arm_macro([](auto& b) {
    // r0 = a, r1 = b, r2 = c, r3 = shift
    b.mul(arm_reg::r1, arm_reg::r0, arm_reg::r1);   // r1 = a * b (note: rd != rm)
    b.lsl_reg(arm_reg::r2, arm_reg::r2, arm_reg::r3); // r2 = c << shift
    b.add_reg(arm_reg::r0, arm_reg::r1, arm_reg::r2); // return sum
    b.bx(arm_reg::lr);
});

alignas(4) unsigned int code[8] = {};
std::memcpy(code, mul_add_shift.data(), mul_add_shift.size_bytes());
auto fn = reinterpret_cast<int (*)(int, int, int, int)>(code);

int result = fn(5, 7, 3, 4); // (5*7) + (3<<4) = 35 + 48 = 83
```

### Conditional branch loop with comparison

Count iterations until a threshold using conditional branching:

```cpp
#include <gba/codegen>

using namespace gba::codegen;

// int count_to_threshold(int start, int threshold)
// Counts how many increments from start to reach threshold
constexpr auto count_to_threshold = arm_macro([](auto& b) {
    // r0 = current value (start)
    // r1 = threshold
    b.mov_imm(arm_reg::r2, 0);           // r2 = count = 0
    // loop_start: (index 2)
    b.cmp_reg(arm_reg::r0, arm_reg::r1); // compare current to threshold
    b.b_if_to(arm_cond::ge, 2, 6);       // if current >= threshold, exit
    b.add_imm(arm_reg::r0, arm_reg::r0, 1); // current++
    b.add_imm(arm_reg::r2, arm_reg::r2, 1); // count++
    b.b_to(5, 2);                        // loop back to cmp
    // loop_exit: (index 6)
    b.mov_reg(arm_reg::r0, arm_reg::r2); // return count
    b.bx(arm_reg::lr);
});

alignas(4) unsigned int code[16] = {};
std::memcpy(code, count_to_threshold.data(), count_to_threshold.size_bytes());
auto fn = reinterpret_cast<int (*)(int, int)>(code);

int result = fn(10, 20); // returns 10 (iterations from 10 to 20)
```

### Bitwise operations: extract and mask fields

Extract a bitfield using shifts and masks:

```cpp
#include <gba/codegen>

using namespace gba::codegen;

// int extract_field(int value, int shift, int mask)
// Returns (value >> shift) & mask
constexpr auto extract_field = arm_macro([](auto& b) {
    // r0 = value, r1 = shift, r2 = mask
    b.lsr_reg(arm_reg::r0, arm_reg::r0, arm_reg::r1); // value >>= shift
    b.and_reg(arm_reg::r0, arm_reg::r0, arm_reg::r2); // value &= mask
    b.bx(arm_reg::lr);
});

alignas(4) unsigned int code[8] = {};
std::memcpy(code, extract_field.data(), extract_field.size_bytes());
auto fn = reinterpret_cast<int (*)(int, int, int)>(code);

int result = fn(0xABCD1234, 8, 0xFF); // returns 0x12
```

### Patchable threshold with runtime branching

A runtime-patchable threshold check that branches to different code paths:

```cpp
#include <gba/codegen>

using namespace gba::codegen;

// int threshold_branch(int value)
// Returns value * 2 if below threshold, else value + 10
constexpr auto threshold_branch = arm_macro([](auto& b) {
    // r0 = input value
    b.cmp_imm(arm_reg::r0, imm_slot(0));     // compare to patchable threshold
    b.b_if_to(arm_cond::ge, 1, 4);           // if >= threshold, skip to else
    // then: value < threshold
    b.add_reg(arm_reg::r0, arm_reg::r0, arm_reg::r0); // value *= 2
    b.b_to(3, 5);                            // skip to return
    // else: value >= threshold (index 4)
    b.add_imm(arm_reg::r0, arm_reg::r0, 10); // value += 10
    // return: (index 5)
    b.bx(arm_reg::lr);
});

alignas(4) unsigned int code[16] = {};

// Install with threshold = 50
auto fn1 = apply_patches<int(int)>(threshold_branch, code, 16, 50u);
int r1 = fn1(30);  // returns 60 (30*2, below threshold)
int r2 = fn1(70);  // returns 80 (70+10, at/above threshold)

// Re-patch with threshold = 100
std::memcpy(code, threshold_branch.data(), threshold_branch.size_bytes());
auto fn2 = apply_patches<int(int)>(threshold_branch, code, 16, 100u);
int r3 = fn2(70);  // returns 140 (70*2, now below threshold)
int r4 = fn2(150); // returns 160 (150+10, at/above threshold)
```

### Performance note

These examples demonstrate codegen patterns commonly needed in GBA software rasterizers, sprite systems, and per-scanline VBlank/HBlank handlers where:

- Code installed once in IWRAM/EWRAM can be re-patched per-frame with different render parameters
- Tight inner loops benefit from specialized instruction sequences (multiply, shifts, conditionals)
- Callee-save conventions enable complex multi-register work without stack spills
- Halfword operations match the GBA's 16-bit VRAM/OAM/palette memory interface
- Conditional branches enable runtime algorithmic adaptation without function call overhead
