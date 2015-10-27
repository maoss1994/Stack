#ifndef _QUEUE_H_
#define _QUEUE_H_

#include <stdlib.h>

#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE*)0)->MEMBER)
#define container(ptr, TYPE, MEMBER)                                    \
        ((TYPE*) ((char*)ptr - offsetof(TYPE, MEMBER)))

typedef struct queue_s queue_t;
struct queue_s {
        queue_t *prev;
        queue_t *next;
};

#define queue_create(q)                                                 \
        ((q) = (queue_t*) malloc(sizeof(queue_t)))

#define queue_init(q)                                                   \
({                                                                      \
        (q)->prev = (q);                                                \
        (q)->next = (q);                                                \
})

#define queue_empty(h)      ((h)->prev == (h))

#define queue_insert_after(h, q)                                        \
({                                                                      \
        (q)->next       = (h)->next;                                    \
        (q)->prev       = (h);                                          \
        (h)->next->prev = (q);                                          \
        (h)->next       = (q);                                          \
})

#define queue_insert(h, q)                                              \
        queue_insert_after(h, q)

#define queue_insert_head(h, q)                                         \
        queue_insert_after(h, q)

#define queue_insert_tail(h, q)                                         \
({                                                                      \
        (q)->next       = (h);                                          \
        (q)->prev       = (h)->prev;                                    \
        (h)->prev->next = (q);                                          \
        (h)->prev       = (q);                                          \
})

#define queue_delete(q)                                                 \
({                                                                      \
        (q)->prev->next = (q)->next;                                    \
        (q)->next->prev = (q)->prev;                                    \
})

#define queue_first(h)      (h)->next

#define queue_last(h)       (h)->prev

#define queue_sentinel(h)   (h)

#define queue_next(q)       (q)->next

#define queue_prev(q)       (q)->prev

#define queue_data(q, TYPE, MEMBER)                                     \
        container(q, TYPE, MEMBER)

#define queue_foreach(h, q, TYPE, MEMBER)                               \
        for (queue_t *_q = (h)->next,                                   \
             (q) = queue_data(_q, TYPE, MEMBER);                        \
             _q != (h);                                                 \
             _q = _q->next,                                             \
             (q) = queue_data(_q, TYPE, MEMBER))

#endif // _QUEUE_H_
