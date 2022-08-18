#ifndef _APPLICATION_H
#define _APPLICATION_H

#include <bcl.h>
#include <twr.h>

typedef struct
{
    uint8_t channel;
    float value;
    twr_tick_t next_pub;

} event_param_t;

#endif // _APPLICATION_H
