#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Shared on the application thread. Saved by the existing Save box settings UI.
namespace MagicSandWaterRipples {
inline bool& enabled(){static bool value=true;return value;}
}
