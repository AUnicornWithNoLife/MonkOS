//============================================================================
/// @file       basic.c
/// @brief      A basic language interator
//
// Copyright 2021 AUnicornWithNoLife.
// Use of this source code is governed by a BSD-style license that can
// be found in the MonkOS LICENSE file.
//============================================================================

#include <core.h>
#include <libc/stdio.h>
#include <libc/stdlib.h>
#include <libc/string.h>
#include <kernel/device/pci.h>
#include <kernel/device/tty.h>
#include <kernel/device/keyboard.h>
#include <kernel/mem/acpi.h>
#include <kernel/mem/heap.h>
#include <kernel/mem/paging.h>
#include <kernel/x86/cpu.h>

#define TTY_CONSOLE  0

static bool interpret()
{
    tty_print(TTY_CONSOLE, kb_getchar());
}