# Memory Utilities

stdgba provides memory management tools designed for the GBA's constrained environment: a bitmap pool allocator, a unique pointer type, and a register-compatible tuple.

## `bitpool` -- bitmap-based allocator

GBA games often need to allocate and free chunks of VRAM for sprite tiles, background maps, or audio buffers. `bitpool` is a fixed-size memory pool backed by a bitmap, providing O(1) allocation and deallocation with zero fragmentation for uniform-sized chunks.

```cpp
#include <gba/memory>

// Create a pool over object VRAM (32 KB with 1 KB chunks)
gba::bitpool vram_pool{gba::memory_map(gba::mem_vram_obj), 1024};

// Allocate 2 KB (2 chunks)
auto tiles = vram_pool.allocate_unique<char>(2048);

// tiles is automatically freed when it goes out of scope
```

### Why not `new`/`delete`?

The GBA has no operating system and only 288 KB of total RAM. General-purpose heap allocators waste memory on metadata and suffer from fragmentation. `bitpool` is deterministic: allocation is a bitmap scan, deallocation is a bit clear. No metadata overhead per allocation.

## `unique<T>` -- owning pointer

`gba::unique<T>` is a lightweight owning pointer that calls a user-provided deleter on destruction. It works with `bitpool::allocate_unique` for RAII-style VRAM management:

```cpp
{
    auto sprite_tiles = vram_pool.allocate_unique<char>(512);
    // Use sprite_tiles...
} // Automatically returned to the pool here
```

## `plex<Ts...>` -- register-compatible tuple

`std::tuple` is not trivially copyable, so it cannot be safely written to hardware registers. `gba::plex` is a fixed-size heterogeneous container (up to 4 bytes total) that is guaranteed trivially copyable:

```cpp
#include <gba/memory>

gba::plex<unsigned short, unsigned short> pair{0x1234, 0x5678};
auto [a, b] = pair;  // Structured bindings

// Can be bit-cast to an integer for register writes
auto as_int = std::bit_cast<unsigned int>(pair);
```

## Optimized `memcpy` / `memset`

stdgba replaces the standard library's `memcpy`, `memmove`, and `memset` with hand-written ARM assembly optimized for the GBA's IWRAM bus:

- **Word-aligned bulk transfers** using `ldmia`/`stmia` (8 registers at a time, 32 bytes per iteration)
- **Alignment promotion** -- unaligned pointers are fixed up with byte/halfword stores before entering the fast path
- **Double-pumped loops** using ARM condition codes to process 64 bytes per loop iteration with no branch overhead

These run in IWRAM for single-cycle instruction access. You do not need to call them directly -- the compiler's `memcpy`/`memset` intrinsics are redirected automatically.
