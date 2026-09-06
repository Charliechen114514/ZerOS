#pragma once

/**
 * @brief Cortex M3 Trap
 *
 * Where a task lands when its entry function returns: the end of the road.
 * Defined in switch.cpp — blocking self there (not squatting in wfi) is
 * what makes a returned task actually leave the cpu.
 */
extern "C" [[noreturn]] void Trap();
