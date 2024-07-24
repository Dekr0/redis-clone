1. Thread Pooling
    - graceful closing when removing a thread pool
2. Event Loop
    - Better memory management for array of `struct pollfd`, 
    `struct rgstr_event_t`, and `fired_event_t`
    - Time Event callback
    - Before Sleep event callback
    - After sleep callback
    - Strip out all unnecessary abstraction from Redis' unstable implementation
        - Goal: an array of `struct pollfd` and an array of `struct` handler 
        map to each `struct pollfd` (1 to 1 mapping). Use I/O's fd to index two 
        of these array.

