#ifndef EVENT_LOOP_H
#define EVENT_LOOP_H

#include <stdint.h>
#include <stdlib.h>
#include <poll.h>

#define E_NONE      0
#define E_READABLE  1
#define E_WRITEABLE 2

#define DEFAULT_FDS_CAP 64

#define OK           0
#define ALLOC_ERR    1
#define RESIZE_ERR   2
#define POLL_ERR     3
#define CRITICAL_ERR 4


struct event_loop_t {
    int32_t               stop;
    int32_t               max_fd;  // Highest file descriptor currently registered
    uint32_t              fds_cap; // Max number of file descriptors tracked
    struct pollfd*        pollfd_arr;      // input for OS polling system call 
    struct rgstr_event_t* rgstr_event_arr; // abstraction input  of OS polling system call
    struct fired_event_t* fired_event_arr; // abstraction output of OS polling system call
};

/** 
 * This should accept one more parameter, client data (buffer for read, IP, ...) 
 * */
typedef void (*event_handle_t)(struct event_loop_t*, int32_t, void *);

/** 
 * An abstract construct for the concept of Event and Polling primitive / 
 * construct, in non-blocking I/O. 
 *
 * Motive: There are different polling mechanisms available in a given OS. Each 
 * polling mechanism has its own polling primitive construct and polling system 
 * call. 
 *
 * Some polling mechanisms are supported in different OS. Some are not. Some are
 * unique to a specific OS (e.g, Window). 
 *
 * How it work ([Un]Register an event):
 * 1. [Un]Mark down what event is being register and what handler will be triggered 
 * when this event is fired
 * 2. Map this abstract construct to the polling primitive construct of the 
 * chosen polling mechanism (in this case `struct pollfd`)
 * 3. To locate the abstract construct and the concrete polling primitive 
 * construct of a given I/O, use its file descriptor to directly index the 
 * construct array for both abstracted one and concrete one.
 *  - The potential problem of this is file descriptor might be larger than 
 *  array size despite the actual size of less than file descriptor number.
 * */
struct rgstr_event_t {
    int16_t        event_mask;
    void*          client_data;
    event_handle_t read_event_handle;  // handle on read
    event_handle_t write_event_handle; // handle on write 
};

/**
 * An simple abstraction (more like a mapping) of the result return from a given 
 * OS polling system call.
 *
 * How it work:
 * 1. Call the system call for the chosen polling mechanism (in this case poll())
 * 2. Once the system call return, iterate through each system polling primitive 
 * construct for its I/O to see whether if it has an event triggered.
 *   - If it has an event triggered, extract the file descriptor of its I/O as 
 *   well as the events triggered into the abstract construct.
 *   - Add this abstract construct into the result array.
 * 3. Iterate through each abstract result construct from the result array.
 *   - Use file descriptor of a given I/O to obtain the abstract construct 
 *   associative with this given I/O.
 *   - Obtain the triggered event and run the associative handler.
 */ 
struct fired_event_t {
    int32_t fd;
    int16_t event_mask;
};

struct event_loop_t* create_event_loop(uint32_t);

void free_event_loop(struct event_loop_t*);

int register_event(struct event_loop_t*, int32_t, int16_t, event_handle_t, void *);

int unregister_event(struct event_loop_t*, int32_t, int16_t);

static int kernel_poll(struct event_loop_t*);

int process_events(struct event_loop_t*);

#endif
