# ARM Codegen

`<gba/codegen>` compiles ARM instruction sequences at C++ `consteval` time,
installs them into executable RAM at runtime, and provides zero-overhead patching
to fill in runtime values without re-copying.

## Quick start

The main power of codegen is **patching**: compile the ARM instruction sequence once,
then replace runtime values (like loop counts, thresholds, or offsets) without re-copying.

```cpp
#include <gba/codegen>
#include <cstring>
using namespace gba::codegen;

// 1. Define a template with a patched constant
static constexpr auto add_const = arm_macro([](auto& b) {
    b.add_imm(arm_reg::r0, arm_reg::r0, imm_slot(0))  // r0 = r0 + (patched value)
     .bx(arm_reg::lr);
});

// 2. Install into executable RAM (once)
alignas(4) std::uint32_t code[add_const.size()] = {};
std::memcpy(code, add_const.data(), add_const.size_bytes());

// 3. Patch and call - reuse the same code buffer with different constants
constexpr block_patcher<add_const> patch{};

auto add_10 = patch.entry<int(int)>(code, 10u);
int result = add_10(5);  // 15 = 5 + 10

auto add_100 = patch.entry<int(int)>(code, 100u);
result = add_100(5);  // 105 = 5 + 100
```

The `imm_slot(0)` marks a placeholder that is filled in at patch time.
No re-copy needed - the same code buffer switches from adding 10 to adding 100.

---

## Building templates

### `arm_macro` (preferred)

```cpp
static constexpr auto tpl = arm_macro([](auto& b) {
    b.mov_imm(arm_reg::r0, 42)
     .bx(arm_reg::lr);
});
```

`arm_macro` infers the required capacity automatically.
All instruction encodings are validated at `consteval` time - invalid operands are
compile errors, not runtime surprises.

### `arm_macro_builder<N>` (explicit capacity)

Use when the capacity must be fixed at the call site, for example inside a `constinit`
variable or a `constexpr` template:

```cpp
constexpr auto tpl = [] {
    auto b = arm_macro_builder<4>{};
    b.mov_imm(arm_reg::r0, 42).bx(arm_reg::lr);
    return b.compile();
}();
```

`b.mark()` returns the current word index - useful for computing forward branch targets
before emitting the branch instruction.

### `compiled_block<N>` accessors

| Member | Type | Description |
|--------|------|-------------|
| `data()` | `const arm_word*` | Pointer to first instruction word |
| `size()` | `std::size_t` | Number of instruction words |
| `size_bytes()` | `std::size_t` | Byte count (`size() * 4`) |
| `operator[]` | `arm_word` | Read a single instruction word |

---

## Patch slots

Patch slots mark positions in the template that are replaced with runtime values
after install.  They accept a slot index `n` (0–31) that maps to a call-site argument.

| Slot | Instruction(s) | Value |
|------|----------------|-------|
| `imm_slot(n)` | `mov_imm`, `add_imm`, `sub_imm`, `orr_imm`, `and_imm`, `eor_imm`, `bic_imm`, `mvn_imm`, `rsb_imm`, `cmp_imm`, `tst_imm` | 0–255 |
| `s12_slot(n)` | `ldr_imm`, `str_imm` | -4095 ... +4095 |
| `b_slot(n)` | `b_to`, `b_if` | 24-bit signed word offset |
| `instr_slot(n)` | `instruction(...)` / `word(...)` / `literal_word(...)` | Any 32-bit word |

`word_slot` and `literal_slot` are aliases for `instr_slot`.

```cpp
static constexpr auto tpl = arm_macro([](auto& b) {
    b.mov_imm(arm_reg::r0, imm_slot(0))              // arg 0 → 8-bit immediate
     .ldr_imm(arm_reg::r1, arm_reg::r2, s12_slot(1)) // arg 1 → ±4095 byte offset
     .instruction(instr_slot(2))                      // arg 2 → full 32-bit word
     .bx(arm_reg::lr);
});
```

---

## Patching

The preferred path uses `block_patcher<tpl>`: a zero-overhead patcher parameterised
on the block value as a non-type template argument.  All patch metadata is a
compile-time constant, so the compiler emits a straight-line sequence of stores --
no loop, no dispatch table, no ROM reads.

### Preferred: `block_patcher<tpl>`

```cpp
static constexpr auto tpl = arm_macro([](auto& b) {
    b.mov_imm(arm_reg::r0, imm_slot(0)).bx(arm_reg::lr);
});

// Declare once as a constexpr object - zero runtime cost
constexpr block_patcher<tpl> patch{};

alignas(4) std::uint32_t code[tpl.size()] = {};
std::memcpy(code, tpl.data(), tpl.size_bytes());

patch(code, 42u);                          // apply patches in place
auto fn = patch.entry<int()>(code, 42u);  // patch + typed function pointer
```

**Typed variant** - when you want `operator()` to return the function pointer:

```cpp
constexpr auto fn_patch = block_patcher<tpl>{}.typed<int()>();
auto fn = fn_patch(code, 42u);  // returns int(*)()
```

### Convenience: `compiled_block::patcher()`

Returns a patcher object obtained directly from the block.
Patch metadata is stored as runtime fields but the patcher is `constexpr`-constructible.
Use this when the block is not a static `constexpr` at the call site, or when
named-argument patching is needed.

```cpp
constexpr auto patch  = tpl.patcher();          // untyped
constexpr auto tpatch = tpl.patcher<int()>();   // typed

std::memcpy(code, tpl.data(), tpl.size_bytes());
patch(code, 42u);                // apply patches only
auto fn = tpatch(code, 42u);    // apply + return int(*)()
```

**Named-argument patching** - order-independent, self-documenting:

```cpp
#include <gba/args>
using namespace gba::literals;

static constexpr auto tpl = arm_macro([](auto& b) {
    b.mov_imm(arm_reg::r0, "x"_arg)
     .add_imm(arm_reg::r0, arm_reg::r0, "y"_arg)
     .bx(arm_reg::lr);
});

constexpr auto patch = tpl.patcher();
std::memcpy(code, tpl.data(), tpl.size_bytes());
patch(code, "y"_arg = 12u, "x"_arg = 30u);  // 42 - order irrelevant
```

### Generic Runtime Dispatch: `apply_patches<Sig>(...)`

Generic runtime function for when the block is not available as a `constexpr` at the call site,
or when patching arguments need to be packed into an array before application.

Variadic form - arguments passed directly:
```cpp
auto fn = apply_patches<int(int)>(tpl, code, tpl.size(), 42u);
```

Packed array form - pre-assembled argument array:
```cpp
std::uint32_t args[] = {30u, 12u};
auto fn = apply_patches_packed<int(int)>(tpl, code, tpl.size(), args, 2);
```

### Whole-instruction patching

Reserve an instruction word and replace it entirely at patch time.
Use the checked helpers to build valid instruction values:

```cpp
static constexpr auto op_tpl = arm_macro([](auto& b) {
    b.mov_imm(arm_reg::r2, imm_slot(0))
     .instruction(instr_slot(1))        // replaced at runtime
     .bx(arm_reg::lr);
});

alignas(4) std::uint32_t code[op_tpl.size()] = {};
std::memcpy(code, op_tpl.data(), op_tpl.size_bytes());

// Pick the operation at runtime
auto add_fn = apply_patches<int(int)>(op_tpl, code, op_tpl.size(),
    5u, add_reg_instr(arm_reg::r0, arm_reg::r0, arm_reg::r2));

auto sub_fn = apply_patches<int(int)>(op_tpl, code, op_tpl.size(),
    5u, sub_reg_instr(arm_reg::r0, arm_reg::r0, arm_reg::r2));
```

Available checked instruction helpers:

```cpp
nop_instr()
add_reg_instr(rd, rn, rm)   sub_reg_instr(rd, rn, rm)
orr_reg_instr(rd, rn, rm)   and_reg_instr(rd, rn, rm)   eor_reg_instr(rd, rn, rm)
lsl_imm_instr(rd, rm, shift)   lsr_imm_instr(rd, rm, shift)
mul_instr(rd, rm, rs)
```

### Callback Patching: `apply_word_patches(...)`

When instruction word patches are generated dynamically at runtime, use the callback-based
`apply_word_patches` function instead of `apply_patches`.  This is useful for multi-operation
switching or complex patch-value computation:

```cpp
static constexpr auto op_tpl = arm_macro([](auto& b) {
    b.mov_imm(arm_reg::r2, imm_slot(0))
     .instruction(instr_slot(1))        // replaced at runtime via callback
     .bx(arm_reg::lr);
});

alignas(4) std::uint32_t code[op_tpl.size()] = {};
std::memcpy(code, op_tpl.data(), op_tpl.size_bytes());

// Use a callback to generate instruction words based on patch index
apply_word_patches(op_tpl, code, op_tpl.size(), [](std::size_t patch_idx) -> std::uint32_t {
    // patch_idx == 1 here (the instruction slot)
    // Return the desired instruction word
    if (some_condition) {
        return add_reg_instr(arm_reg::r0, arm_reg::r0, arm_reg::r2);
    } else {
        return sub_reg_instr(arm_reg::r0, arm_reg::r0, arm_reg::r2);
    }
});
```

---

## Instruction reference

All instructions are available as builder methods on `arm_macro_builder<N>` and
accepted by the `arm_macro` lambda.

### Data movement

| Builder method | Effect |
|----------------|--------|
| `mov_imm(rd, imm8)` | `rd = imm8` (0–255) |
| `mov_imm(rd, imm_slot(n))` | `rd = arg[n]` at patch time |
| `mov_reg(rd, rm)` | `rd = rm` |

### Arithmetic

| Method | Effect | Patch variant |
|--------|--------|---------------|
| `add_imm(rd, rn, imm8)` | `rd = rn + imm8` | `imm_slot` |
| `add_reg(rd, rn, rm)` | `rd = rn + rm` | |
| `sub_imm(rd, rn, imm8)` | `rd = rn - imm8` | `imm_slot` |
| `sub_reg(rd, rn, rm)` | `rd = rn - rm` | |
| `rsb_imm(rd, rn, imm8)` | `rd = imm8 - rn` | `imm_slot` |
| `rsb_reg(rd, rn, rm)` | `rd = rm - rn` | |
| `adc_imm(rd, rn, imm8)` | `rd = rn + imm8 + C` | |
| `adc_reg(rd, rn, rm)` | `rd = rn + rm + C` | |
| `sbc_imm(rd, rn, imm8)` | `rd = rn - imm8 - !C` | |
| `sbc_reg(rd, rn, rm)` | `rd = rn - rm - !C` | |

### Bitwise

| Method | Effect | Patch variant |
|--------|--------|---------------|
| `orr_imm(rd, rn, imm8)` | `rd = rn \| imm8` | `imm_slot` |
| `orr_reg(rd, rn, rm)` | `rd = rn \| rm` | |
| `and_imm(rd, rn, imm8)` | `rd = rn & imm8` | `imm_slot` |
| `and_reg(rd, rn, rm)` | `rd = rn & rm` | |
| `eor_imm(rd, rn, imm8)` | `rd = rn ^ imm8` | `imm_slot` |
| `eor_reg(rd, rn, rm)` | `rd = rn ^ rm` | |
| `bic_imm(rd, rn, imm8)` | `rd = rn & ~imm8` | `imm_slot` |
| `bic_reg(rd, rn, rm)` | `rd = rn & ~rm` | |
| `mvn_imm(rd, imm8)` | `rd = ~imm8` | `imm_slot` |
| `mvn_reg(rd, rm)` | `rd = ~rm` | |

### Shifts and rotates

| Method | Shift amount | Range |
|--------|--------------|-------|
| `lsl_imm(rd, rm, shift)` | Immediate | 0–31 |
| `lsr_imm(rd, rm, shift)` | Immediate | 1–32 |
| `asr_imm(rd, rm, shift)` | Immediate | 1–32 |
| `ror_imm(rd, rm, shift)` | Immediate | 1–31 |
| `lsl_reg(rd, rm, rs)` | Register `rs` | |
| `lsr_reg(rd, rm, rs)` | Register `rs` | |
| `asr_reg(rd, rm, rs)` | Register `rs` | |
| `ror_reg(rd, rm, rs)` | Register `rs` | |

### Comparison / flag-setting

These set CPSR flags without writing a destination register.

| Method | Flags set on |
|--------|--------------|
| `cmp_imm(rn, imm8)` / `cmp_reg(rn, rm)` | `rn - operand` |
| `cmn_imm(rn, imm8)` / `cmn_reg(rn, rm)` | `rn + operand` |
| `tst_imm(rn, imm8)` / `tst_reg(rn, rm)` | `rn & operand` |
| `teq_imm(rn, imm8)` / `teq_reg(rn, rm)` | `rn ^ operand` |

`cmp_imm` and `tst_imm` also accept `imm_slot(n)`.

### Memory - word and byte

| Method | Access |
|--------|--------|
| `ldr_imm(rd, rn, offset)` / `str_imm(rd, rn, offset)` | 32-bit word, offset -4095...+4095; accepts `s12_slot` |
| `ldrb_imm(rd, rn, offset)` / `strb_imm(rd, rn, offset)` | Unsigned byte, immediate offset |
| `ldrb_reg(rd, rn, rm)` / `strb_reg(rd, rn, rm)` | Unsigned byte, register offset |

### Memory - halfword and signed forms

| Method | Access |
|--------|--------|
| `ldrh_imm(rd, rn, offset)` / `strh_imm(rd, rn, offset)` | Unsigned halfword, immediate offset |
| `ldrh_reg(rd, rn, rm)` / `strh_reg(rd, rn, rm)` | Unsigned halfword, register offset |
| `ldrsb_imm(rd, rn, offset)` / `ldrsb_reg(rd, rn, rm)` | Signed byte |
| `ldrsh_imm(rd, rn, offset)` / `ldrsh_reg(rd, rn, rm)` | Signed halfword |

### Multi-register and stack

Build a register bitmask with `reg_list(r0, r4, lr, ...)`.

| Method | ARM mnemonic |
|--------|--------------|
| `push(regs)` | `STMDB SP!, {regs}` |
| `pop(regs)` | `LDMIA SP!, {regs}` |
| `ldmia(rn, regs [,wb])` | `LDMIA rn[!], {regs}` |
| `stmia(rn, regs [,wb])` | `STMIA rn[!], {regs}` |
| `ldmib(rn, regs [,wb])` | `LDMIB rn[!], {regs}` |
| `stmib(rn, regs [,wb])` | `STMIB rn[!], {regs}` |
| `ldmda(rn, regs [,wb])` | `LDMDA rn[!], {regs}` |
| `stmda(rn, regs [,wb])` | `STMDA rn[!], {regs}` |
| `ldmdb(rn, regs [,wb])` | `LDMDB rn[!], {regs}` |
| `stmdb(rn, regs [,wb])` | `STMDB rn[!], {regs}` |

```cpp
b.push(reg_list(arm_reg::r4, arm_reg::r5, arm_reg::lr));
// ... body ...
b.pop(reg_list(arm_reg::r4, arm_reg::r5, arm_reg::pc));
```

### Multiply

> **ARM7TDMI constraint:** `rd` must differ from `rm`.

| Method | Effect |
|--------|--------|
| `mul(rd, rm, rs)` | `rd = rm * rs` |
| `mla(rd, rm, rs, rn)` | `rd = rm * rs + rn` |

### Branches

| Method | Effect |
|--------|--------|
| `b_to(target)` | Unconditional, by word index |
| `b_to(b_slot(n))` | Patchable branch offset |
| `b_if(cond, target)` | Conditional, by word index |
| `b_if(cond, b_slot(n))` | Patchable conditional branch |
| `bl_to(target)` | Branch with link |
| `bx(rm)` | Branch exchange - use for function returns |
| `blx(rm)` | Branch exchange with link |

`arm_cond` values:
`eq` `ne` `cs`/`hs` `cc`/`lo` `mi` `pl` `vs` `vc` `hi` `ls` `ge` `lt` `gt` `le` `al`

#### Branching patterns

`b_to` and `b_if` take a *target word index* - the index of the instruction you want
to jump to.  Use `b.mark()` to read the current word index at any point during
construction:

```cpp
// Loop: count down from r0 to zero
const auto loop_top = b.mark();          // remember top of loop
b.sub_imm(arm_reg::r0, arm_reg::r0, 1); // r0--
b.cmp_imm(arm_reg::r0, 0);
b.b_if(arm_cond::ne, loop_top);         // branch back while r0 != 0
b.bx(arm_reg::lr);
```

For forward branches, emit the branch first, then record where the target lands:

```cpp
b.cmp_imm(arm_reg::r0, 100);
const auto branch_instr = b.mark();      // index of the b_if we're about to emit
b.b_if(arm_cond::ge, 0);                // target unknown yet - placeholder
b.add_imm(arm_reg::r0, arm_reg::r0, 5); // only reached when r0 < 100
// ... forward code goes here ...
```

> **Note:** Forward branches where the target index is not yet known require
> `arm_macro_builder<N>` with explicit capacity, since you need to emit the branch
> before you know the target.  With `arm_macro` you can structure control flow so
> that all targets are emitted before the branch (back-branches) or known from
> `b.mark()` arithmetic.

---

## AAPCS calling convention

Generated leaf functions receive and return values through the standard ARM AAPCS
convention used on GBA.  No special setup is needed - just cast the destination
pointer to the right type.

| Role | Register |
|------|----------|
| Argument 0 | `r0` |
| Argument 1 | `r1` |
| Argument 2 | `r2` |
| Argument 3 | `r3` |
| Return value | `r0` |

Register-form instructions (`add_reg`, `sub_reg`, `mul`, ...) operate directly on
call-time arguments without any patch slots.

---

## Examples

### Patched constant (simplest case)

This is the Quick start pattern - add a call-time argument to a patched constant:

```cpp
static constexpr auto add_const = arm_macro([](auto& b) {
    b.add_imm(arm_reg::r0, arm_reg::r0, imm_slot(0))
     .bx(arm_reg::lr);
});

alignas(4) std::uint32_t code[add_const.size()] = {};
std::memcpy(code, add_const.data(), add_const.size_bytes());

constexpr block_patcher<add_const> patch{};
auto fn = patch.entry<int(int)>(code, 42u);
int result = fn(8);  // 50 = 8 + 42
```

### Function with two call-time arguments

Both arguments come through AAPCS registers; no patching needed:

```cpp
static constexpr auto add_fn = arm_macro([](auto& b) {
    b.add_reg(arm_reg::r0, arm_reg::r0, arm_reg::r1)
     .bx(arm_reg::lr);
});

alignas(4) std::uint32_t code[add_fn.size()] = {};
std::memcpy(code, add_fn.data(), add_fn.size_bytes());
auto fn = reinterpret_cast<int (*)(int, int)>(code);
int result = fn(30, 12);  // 42
```

### Loop with patched iteration count

Count down from a patched limit:

```cpp
// int countdown_by_step(int start) - counts down with a patched step size
static constexpr auto countdown_loop = arm_macro([](auto& b) {
    b.mov_imm(arm_reg::r1, 0);                        // count = 0
    const auto loop_start = b.mark();                 // loop top: index 1
    b.sub_imm(arm_reg::r0, arm_reg::r0, imm_slot(0)); // start -= step_size (patched)
    b.add_imm(arm_reg::r1, arm_reg::r1, 1);           // count++
    b.cmp_imm(arm_reg::r0, 0);                        // if start <= 0, exit
    b.b_if(arm_cond::gt, loop_start);                 // if start > 0, loop
    b.mov_reg(arm_reg::r0, arm_reg::r1);              // return count
    b.bx(arm_reg::lr);
});

alignas(4) std::uint32_t code[countdown_loop.size()] = {};
std::memcpy(code, countdown_loop.data(), countdown_loop.size_bytes());

constexpr block_patcher<countdown_loop> patch{};

// Patch step size = 1
auto count_by_1 = patch.entry<int(int)>(code, 1u);
int loops_by_1 = count_by_1(10);  // 10 iterations: 10, 9, 8, ..., 1, 0

// Re-patch: step size = 2 (no re-copy needed!)
auto count_by_2 = patch.entry<int(int)>(code, 2u);
int loops_by_2 = count_by_2(10);  // 5 iterations: 10, 8, 6, 4, 2, 0
```

### Mixed: call-time arguments and patch-time constant

```cpp
// x * 4 + c  - x is a call-time argument, c is patched in
static constexpr auto scale_add = arm_macro([](auto& b) {
    b.add_reg(arm_reg::r0, arm_reg::r0, arm_reg::r0) // *2
     .add_reg(arm_reg::r0, arm_reg::r0, arm_reg::r0) // *4
     .add_imm(arm_reg::r0, arm_reg::r0, imm_slot(0)) // + c
     .bx(arm_reg::lr);
});

constexpr block_patcher<scale_add> patch{};

alignas(4) std::uint32_t code[scale_add.size()] = {};
std::memcpy(code, scale_add.data(), scale_add.size_bytes());

auto fn = patch.entry<int(int)>(code, 2u);  // 4x + 2
int r = fn(10);  // 42
```

### Callee-save register pattern

```cpp
// int compute(int a, int b, int c)  -  (a * b) + (c << 2)
static constexpr auto compute = arm_macro([](auto& b) {
    b.push(reg_list(arm_reg::r4, arm_reg::lr));
    b.mul(arm_reg::r4, arm_reg::r0, arm_reg::r1); // r4 = a * b  (r4 != r0)
    b.lsl_imm(arm_reg::r0, arm_reg::r2, 2);       // r0 = c << 2
    b.add_reg(arm_reg::r0, arm_reg::r4, arm_reg::r0);
    b.pop(reg_list(arm_reg::r4, arm_reg::pc));
});
```

### Conditional loop with comparison

```cpp
// Count iterations from `start` until value reaches `limit`
static constexpr auto count_loop = arm_macro([](auto& b) {
    b.mov_imm(arm_reg::r2, 0);              // count = 0; index 0
    // loop top: index 1
    b.cmp_reg(arm_reg::r0, arm_reg::r1);
    b.b_if(arm_cond::ge, 5);               // exit if r0 >= limit; index 2
    b.add_imm(arm_reg::r0, arm_reg::r0, 1);// r0++; index 3
    b.add_imm(arm_reg::r2, arm_reg::r2, 1);// count++; index 4
    b.b_to(1);                             // back to loop top; index 5 - exit
    b.mov_reg(arm_reg::r0, arm_reg::r2);   // return count; index 6
    b.bx(arm_reg::lr);
});
```

### Patchable threshold

```cpp
// Returns value * 2 if below threshold, value + 10 otherwise
static constexpr auto threshold_fn = arm_macro([](auto& b) {
    b.cmp_imm(arm_reg::r0, imm_slot(0));          // index 0
    b.b_if(arm_cond::ge, 3);                       // index 1 - skip to else
    b.add_reg(arm_reg::r0, arm_reg::r0, arm_reg::r0); // *2; index 2
    b.b_to(4);                                     // index 3 - skip else
    b.add_imm(arm_reg::r0, arm_reg::r0, 10);       // +10; index 4
    b.bx(arm_reg::lr);                             // index 5
});

alignas(4) std::uint32_t code[threshold_fn.size()] = {};
std::memcpy(code, threshold_fn.data(), threshold_fn.size_bytes());

// Install with threshold = 50; re-patch any time without re-copying
constexpr block_patcher<threshold_fn> patch{};
auto fn = patch.entry<int(int)>(code, 50u);
```

### Halfword OAM update (GBA sprite system)

```cpp
// void update_sprite(volatile std::uint16_t* oam, int x, int y)
static constexpr auto update_sprite = arm_macro([](auto& b) {
    // attr0: clear Y field, insert new Y
    b.ldrh_imm(arm_reg::r3, arm_reg::r0, 0);
    b.bic_imm(arm_reg::r3, arm_reg::r3, 0xFF);
    b.orr_reg(arm_reg::r3, arm_reg::r3, arm_reg::r2);
    b.strh_imm(arm_reg::r3, arm_reg::r0, 0);
    // attr1: clear X field, insert new X
    b.ldrh_imm(arm_reg::r3, arm_reg::r0, 2);
    b.bic_imm(arm_reg::r3, arm_reg::r3, 0xFF);
    b.orr_reg(arm_reg::r3, arm_reg::r3, arm_reg::r1);
    b.strh_imm(arm_reg::r3, arm_reg::r0, 2);
    b.bx(arm_reg::lr);
});
```

---

## Safety notes

- The destination buffer must be **word-aligned** (`alignas(4)`) and located in
  **executable RAM** (IWRAM or EWRAM on GBA).
- Encoding errors (immediate out of range, invalid register combination) are
  **compile errors** in `consteval` context.
- `b_to` / `b_if` targets are in **instruction words**, not bytes.
- `mul` / `mla`: `rd ≠ rm` (ARM7TDMI hardware constraint).
- These APIs cover leaf-function patterns (AAPCS `r0`–`r3` arguments, `r0` return).
  Stack-passed arguments, calls to other functions, and floating-point are not abstracted.
