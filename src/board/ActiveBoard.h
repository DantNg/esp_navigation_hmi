/**
 * @file ActiveBoard.h
 * @brief Compile-time selection of the active board description.
 *
 * Pick a board by defining its build flag in platformio.ini (one env per
 * board). The default — no flag — is the CrowPanel, keeping existing builds
 * unchanged. `board::kBoard` is the single source of truth every other module
 * reads its hardware parameters from.
 *
 *   -D BOARD_GUITION_JC8048W550   -> GUITION JC8048W550
 *   (none)                        -> Elecrow CrowPanel 5.0"
 */
#ifndef BOARD_ACTIVE_BOARD_H
#define BOARD_ACTIVE_BOARD_H

#if defined(BOARD_GUITION_JC8048W550)
#  include "board/boards/Guition_JC8048W550.h"
namespace board { inline constexpr BoardConfig kBoard = boards::kGuitionJC8048W550; }
#else
#  include "board/boards/CrowPanel_5_0.h"
namespace board { inline constexpr BoardConfig kBoard = boards::kCrowPanel50; }
#endif

#endif /* BOARD_ACTIVE_BOARD_H */
