#pragma once

#include "machineprimitives.h"

struct MiniThread
{
    stack_pointer_t stack_base;
    stack_pointer_t stack_top;
};
