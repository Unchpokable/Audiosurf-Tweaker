#pragma once

#include "engine/channel_ref.hxx"

// Tables: a column channel plus the cursor channel that picks its row. Not a family of its own - an
// `Array Value` column is a number and an `Array Vector` column is a vector - but a mechanism layered
// over two families, with a hazard neither of them has.
//
// HOW THE ENGINE ADDRESSES A ROW (engine journal §5). A column takes its row index from the separate
// channel wired to its port 0, the cursor. Reading row n is: move the cursor to n, read the column,
// put the cursor back. The putting back is not tidiness: the cursor is shared with the game, and
// leaving it moved corrupts whatever the graph does next with it, not just what we read.
//
// THE HAZARD (engine journal §2.2.3). The engine's row setter, ArrayConnectItem::SetData, asks its
// column for the row and **creates it when it is missing**. So an out-of-range write is not a no-op
// and not a wrong-cell write - it silently lengthens a table the rest of the game reads. Every write
// here therefore checks the row exists first, through the connect item's own GetRow (+0x48), which
// sits right next to GetRowOrCreate (+0x44) with the same signature. Swapping those two turns the
// check into the thing it guards against; harness/lua/arraytest pins it.
//
// The cell itself is read and written through fam_number / fam_vector - Aco_Array_Value overrides
// slot 19 and Aco_Array_Vector slot 18 to be the table write, so no thunk of its own is needed here.
namespace tw::engine::fam_table
{
// One cell of an Array Value column (a number-family ref) through its cursor (a number-family ref).
// The column's memo is busted before the read when it has one: most Array Value channels ship with
// CHIC = 1 and are never memoised, but for the rest the second read in a frame would otherwise return
// the first row's value. 0 when either ref is of the wrong family.
[[nodiscard]] float read(const channel_ref& column, const channel_ref& cursor, float index) noexcept;

// The same for an Array Vector column (a vector-family ref). False, with `out` untouched, when either
// ref is of the wrong family.
bool read_vector(const channel_ref& column, const channel_ref& cursor, float index, float out[3]) noexcept;

// How many rows the table behind the column has, or -1 when the column is not an Array Value /
// Array Vector or its table will not connect. The number that makes writing safe at all.
[[nodiscard]] int row_count(const channel_ref& column) noexcept;

// Whether `row` exists **without creating it**.
[[nodiscard]] bool has_row(const channel_ref& column, int row) noexcept;

// Writes one cell, refusing a row that does not exist. Two things differ from the read path on
// purpose:
//
//  - no memo bust before the write - the setters never consult CheckRenderCount, so a write always
//    lands;
//  - a memo bust **after** it - both setters also store the value into the channel's own scalar as a
//    side effect, and a channel the game had already evaluated this frame would hand that scalar to
//    its next reader regardless of the cursor.
//
// False means the write did not happen. True means the setter ran with the cursor on the requested
// row, which is as much as can be promised: the engine's setters return nothing.
bool write(const channel_ref& column, const channel_ref& cursor, float index, float value) noexcept;
bool write_vector(const channel_ref& column, const channel_ref& cursor, float index, float x, float y, float z) noexcept;
} // namespace tw::engine::fam_table
