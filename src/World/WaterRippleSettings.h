/* TopoPlay modification notice — 2026-10-23
 * Added in the TopoPlay educational fork.
 * This update adds source-release licensing/provenance notices.
 * Earlier local development dates were not individually recorded.
 * Original notices below are retained; see docs/MODIFICATIONS.md.
 */
#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Shared on the application thread. Saved by the existing Save box settings UI.
namespace MagicSandWaterRipples {
inline bool& enabled(){static bool value=true;return value;}
}
