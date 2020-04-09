///
/// Created by Mitch on 04/02/2020
///

#include "heartbeat.h"
#include <stdio.h>
#include <time.h>

const int NUM_SECONDS = 10;

void HeartbeatInit()
{
    /// Make initial connection to backend services to get UUID, then call Heartbeat()
}

void Heartbeat()
{
    double time_counter = 0;

    clock_t this_time = clock();
    clock_t last_time = this_time;

    while(true)
    {
        this_time = clock();
        time_counter += (double)(this_time - last_time);
        last_time = this_time;

        if(time_counter > (double)(NUM_SECONDS * CLOCKS_PER_SEC))
        {
            time_counter -= (double)(NUM_SECONDS * CLOCKS_PER_SEC);


        }
    }
}