/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information.

#pragma once

#include "machineprimitives.h"

struct MiniThread
{
    stack_pointer_t stack_base;
    stack_pointer_t stack_top;
};
