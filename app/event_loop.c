#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/types.h>

#include "event_loop.h"


struct event_loop_t* create_event_loop(uint32_t fds_cap) {
    struct event_loop_t* el = NULL;
    
    if ((el = calloc(1, sizeof(struct event_loop_t))) == NULL) {
#ifdef DEBUG
        printf("[create_event_loop] event_loop_t calloc error\n");
#endif /* ifdef DEBUG */
        return NULL;
    }

    if ((el->rgstr_event_arr = calloc(fds_cap, sizeof(struct rgstr_event_t))) == NULL) {
#ifdef DEBUG
        printf("[create_event_loop] rgstr_even_t* calloc error\n");
#endif /* ifdef DEBUG */
        free(el);
        return NULL;
    }

    if ((el->fired_event_arr = calloc(fds_cap, sizeof(struct fired_event_t))) 
            == NULL) {
#ifdef DEBUG
        printf("[create_event_loop] struct fired_event_t* calloc error\n");
#endif /* ifdef DEBUG */
        free(el->rgstr_event_arr);
        free(el);
        return NULL;
    }

    el->fds_cap = fds_cap;
    el->max_fd  = -1;
    el->stop    = 0;

    if ((el->pollfd_arr = calloc(fds_cap, sizeof(struct pollfd))) == NULL) {
#ifdef DEBUG
        printf("[create_event_loop] struct pollfd* calloc error\n");
#endif /* ifdef DEBUG */
        free(el->fired_event_arr);
        free(el->rgstr_event_arr);
        free(el);
        return NULL;
    }

    for (uint32_t i = 0; i < fds_cap; i++) {
        el->rgstr_event_arr[i].event_mask = E_NONE;
    }
#ifdef DEBUG
    printf("[create_event_loop] OK. Handling capacity: %d\n", el->fds_cap);
#endif /* ifdef DEBUG */

    return el;
}

void free_event_loop(struct event_loop_t* el) {
    assert(el != NULL);
    assert(el->fired_event_arr != NULL);
    assert(el->pollfd_arr != NULL);
    assert(el->rgstr_event_arr != NULL);

    free(el->fired_event_arr);
    free(el->pollfd_arr);
    free(el->rgstr_event_arr);
    free(el);
}

/** TODO: This reallocation need to be done in a better way */
static int resize_event_loop(struct event_loop_t* el, uint32_t fds_cap) {
    assert(el != NULL);

    if (fds_cap == el->fds_cap) return OK;
    if (fds_cap <= el->max_fd) return RESIZE_ERR;

    void* try = realloc(el->pollfd_arr, sizeof(struct pollfd) * fds_cap);
    if (try == NULL) {
#ifdef DEBUG
        printf("[resize_event_loop] pollfd* realloc error\n");
#endif /* ifdef DEBUG */
        return ALLOC_ERR;
    }
    el->pollfd_arr = try;

    try = realloc(el->rgstr_event_arr, sizeof(struct rgstr_event_t) * fds_cap);
    if (try == NULL) {
#ifdef DEBUG
        printf("[resize_event_loop] rgstre_event_t* realloc error\n");
#endif /* ifdef DEBUG */
        if ((el->pollfd_arr = realloc(el->pollfd_arr, el->fds_cap * 
                        sizeof(struct pollfd))) 
                == NULL) {
            return CRITICAL_ERR;
        }
        return ALLOC_ERR;
    }

    try = realloc(el->fired_event_arr, sizeof(struct fired_event_t) * fds_cap);
    if (try == NULL) {
#ifdef DEBUG
        printf("[resize_event_loop] rgstre_event_t* realloc error\n");
#endif /* ifdef DEBUG */
        if ((el->pollfd_arr = realloc(el->pollfd_arr, el->fds_cap * 
                        sizeof(struct pollfd)))
                == NULL) {
            return CRITICAL_ERR;
        }
        if ((el->rgstr_event_arr = realloc(el->pollfd_arr, el->fds_cap * 
                        sizeof(struct pollfd)))
                == NULL) {
            return CRITICAL_ERR;
        }
        return ALLOC_ERR;
    }

    el->fds_cap = fds_cap;

    for (uint32_t i = 0; i < el->max_fd + 1; i++) {
        el->pollfd_arr[i].fd = 0;
        el->pollfd_arr[i].events = E_NONE;
        el->pollfd_arr[i].revents = 0;

        el->rgstr_event_arr[i].event_mask = E_NONE;

        el->fired_event_arr[i].fd = 0;
        el->fired_event_arr[i].event_mask = E_NONE;
    }

    return OK;
}

int register_event(struct event_loop_t* el, int32_t fd, int16_t event_mask, 
        event_handle_t h, void* client_data) {
    assert(el != NULL);

    // This should only happen when high load happen.
    if (fd >= el->fds_cap) {
        int32_t factor = fd / el->fds_cap;
        assert(factor >= 1);
        int rval = OK;
        if ((rval = resize_event_loop(el, (factor + 1) * el->fds_cap)) != OK) {
            return rval;
        }
    }

    struct rgstr_event_t* re = &el->rgstr_event_arr[fd];
    struct pollfd* pfd = &el->pollfd_arr[fd];
    assert(re != NULL && pfd != NULL);


    /** Reflect / Map to concrete polling primitive construct */
    pfd->fd = fd;
    if (event_mask & E_READABLE) {
        pfd->events |= POLLIN;
    }
    if (event_mask & E_WRITEABLE) {
        pfd->events |= POLLOUT;
    }
    pfd->revents = 0;


    /** Assignment for abstraction */
    re->event_mask |= event_mask;
    if (event_mask & E_READABLE) {
        re->read_event_handle = h;
    }
    if (event_mask & E_WRITEABLE) {
        re->write_event_handle = h;
    }
    re->client_data = client_data;

    if (fd > el->max_fd) {
        el->max_fd = fd;
    }

    return OK;
}

int unregister_event(struct event_loop_t* el, int32_t fd, 
        int16_t del_event_mask) {
    assert(el != NULL);

    if (fd >= el->fds_cap) return ERANGE;

    struct rgstr_event_t* re = &el->rgstr_event_arr[fd];
    struct pollfd* pfd = &el->pollfd_arr[fd];
    assert(re != NULL && pfd != NULL);

    if (re->event_mask == E_NONE) return OK;


    /** Reflect / Map to concrete polling primitive construct */
    int16_t event_mask = re->event_mask & ~del_event_mask;
    pfd->events = 0;
    if (event_mask & E_READABLE) { pfd->events |= POLLIN; } 
    if (event_mask & E_WRITEABLE) { pfd->events |= POLLOUT; }
    pfd->fd = 0;

    /** Assignment for abstraction */
    re->event_mask = re->event_mask & (~del_event_mask);

    if (fd == el->max_fd && re->event_mask == E_NONE) {
        uint32_t i = 0;
        for (i = el->max_fd - 1; i >= 0; i--) {
            if (el->rgstr_event_arr[i].event_mask != E_NONE) {
                break;
            }
        }
        el->max_fd = i;
    }

    // TODO: shrink size based on some strategies
    return OK;
}

static int kernel_poll(struct event_loop_t* el) {
    assert(el != NULL);

    int32_t rval = 0;
    int32_t numevents = 0;

    rval = poll(el->pollfd_arr, el->max_fd + 1, 10000);

    if (rval == -1) { 
        printf("poll() error %s\n", strerror(errno));
        return POLL_ERR;
    }

    if (rval == 0) { return 0; }

    int16_t event_mask = 0;
    uint32_t i = 0;
    struct pollfd* pfd = NULL;
    for (i = 0; i <= el->max_fd; i++) { 
        event_mask = 0;

        pfd = &el->pollfd_arr[i];
        assert(pfd != NULL);

#ifdef DEBUG
            printf("[kernel_poll] Checking I/O %d\n", i);
#endif /* ifdef DEBUG */

        if (pfd->revents & POLLIN) { event_mask = E_READABLE; }
        if (pfd->revents & POLLOUT) { event_mask = E_WRITEABLE; }
        if (pfd->revents & POLLERR) { event_mask |= E_WRITEABLE | E_READABLE; }
        if (pfd->revents & POLLHUP) { event_mask |= E_WRITEABLE | E_READABLE; }

        if (event_mask != E_NONE) {
#ifdef DEBUG
            printf("[kernel_poll] I/O %d is active (mask = %d)\n", pfd->fd, 
                    event_mask);
#endif /* ifdef DEBUG */

            el->fired_event_arr[numevents].fd = pfd->fd;
            el->fired_event_arr[numevents].event_mask = event_mask;

            numevents++;
        }
    }

#ifdef DEBUG
    printf("[kernel_poll] OK. Number of active I/O events: %d\n", numevents);
#endif /* ifdef DEBUG */

    return numevents;
}

int process_events(struct event_loop_t* el) {
    assert(el != NULL);

    uint32_t processed = 0;
    int32_t numevents = 0;

    /** TODO: time event callback */
    /** TODO: before sleep event callback */
    numevents = kernel_poll(el);
    if (numevents == POLL_ERR) { return POLL_ERR; }
    /** TODO: after sleep event callback */

    
    /** Run fired callback */
    int8_t fired = 0;
    int32_t fd = 0;
    int16_t event_mask = 0;
    struct rgstr_event_t* re = NULL;
    for (uint32_t i = 0; i < numevents; i++) {
        fd = el->fired_event_arr[i].fd;

        re = &el->rgstr_event_arr[fd];
        assert(re != NULL);

        event_mask = el->fired_event_arr[i].event_mask;

        fired = 0;
        
        if (re->event_mask & event_mask & E_READABLE) {
#ifdef DEBUG
            printf("[process_events] Handle I/O %d event %d\n", fd, event_mask);
#endif /* ifdef DEBUG */
            re->read_event_handle(el, fd, re->client_data);
            fired++;
        }
        if (re->event_mask & event_mask & E_WRITEABLE) {
            if (!fired || re->read_event_handle != re->write_event_handle) {
                re->write_event_handle(el, fd, re->client_data);
#ifdef DEBUG
            printf("[process_events] Handle I/O %d event %d\n", fd, event_mask);
#endif /* ifdef DEBUG */
                fired++;
            }
        }

        processed++;
    }

    /** TODO: check time events */

    return processed;
}
