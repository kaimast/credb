/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information.

#pragma once

#include <vector>
#include "PendingResponse.h"

namespace credb
{
namespace trusted
{

inline bool wait_for(std::vector<PendingBooleanResponse> &responses, Task &task)
{
    bool result = true;

    for(auto &pending: responses)
    {
        auto &peer = pending.peer();
        peer.lock();

        // check before we suspend
        pending.wait(false);
        
        while(!pending.has_message())
        {
            peer.unlock();
            task.suspend();
            peer.lock();

            pending.wait(false);
        }

        peer.unlock();

        if(!pending.result())
        {
            result = false;
        }
    }

    responses.clear();
    return result;
}

inline bool wait_for(std::vector<PendingWitnessResponse> &responses, Task &task)
{
    bool result = true;

    for(auto &pending: responses)
    {
        auto &peer = pending.peer();
        peer.lock();

        // check before we suspend
        pending.wait(false);
        
        while(!pending.has_message())
        {
            peer.unlock();
            task.suspend();
            peer.lock();

            pending.wait(false);
        }

        peer.unlock();

        if(!pending.success())
        {
            result = false;
        }
    }

    responses.clear();
    return result;
}

}
}
