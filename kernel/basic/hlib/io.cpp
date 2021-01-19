//============================================================================
/// @file       io.cpp
/// @brief      Simple kernel shell for testing purposes.
//
// Copyright 2021 AUnicornWithNoLife.
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

class String
{
    public:
        const char *characters;
        int length;

        String(const char input[])
        {
            length = sizeof(input);

            characters = new char[length];
            characters = input;
        }
};

void PrintLine(String line = "")
{
    tty_print(TTY_CONSOLE, line.characters);
    tty_print(TTY_CONSOLE, String("\n").characters);
}

void Print(String line = "")
{
    tty_print(TTY_CONSOLE, line.characters);
}