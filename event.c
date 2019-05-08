/*
    ArenaDB event loop implementation. 5.2
*/

#include <stdlib.h>
#include "event.h"

// Create an event loop that can handle up to 'set_size' events simultaneously
ae_event_loop *ae_create_event_loop(int set_size)
{
    ae_event_loop *el = malloc(sizeof(ae_event_loop));
    el->events = malloc(sizeof(ae_file_event) * set_size);
    el->fired = malloc(sizeof(ae_fired_event) * set_size);

    for (int i = 0; i < set_size; i ++) {
        el->events[i].state_mask = AE_STATE_NONE;
    }

    return el;
}
