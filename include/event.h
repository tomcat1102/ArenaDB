#ifndef EVENT_H_INCLUDED
#define EVENT_H_INCLUDED

// Execution codes
#define AE_OK   0
#define AE_ERR  1

// Event states.
#define AE_STATE_NONE       0
#define AE_STATE_READABLE   1
#define AE_STATE_WRITABLE   2

// Event types
#define AE_EVENT_FILE       1
#define AE_EVENT_TIME       2

struct ae_event_loop;//forward declaration for compilation

// Procedure prototypes
typedef void ae_file_proc(struct ae_event_loop *event_loop, int fd, int state);

// File event that can be registered to event loop and then monitored for the desired state to become ture.
typedef struct ae_file_event {
    int state_mask;     // States that is being monitored. Can be AE_STATE_READBLE or AE_STATE_WRITABLE
    ae_file_proc *proc; // Callback function that is invoked when desired state in the mask becomes true.
} ae_file_event;

// When a file or time event happens, a fired event is generated and later processed.
typedef struct ae_fired_event {
    int fd;
    int state;
} ae_fired_event;

// Event loop structure that holds all info about the loop.
typedef struct ae_event_loop {
    int max_fd;             // Highest fd currently registered;
    int set_size;           // Size of the file event set
    ae_file_event *events;  // Registered file events
    ae_fired_event *fired;  // Fired events
    void *api_data;         // API data for select, kquene or epoll, etc.
} ae_event_loop;


typedef struct ae_event_loop ae_event_loop;





#endif // EVENT_H_INCLUDED
