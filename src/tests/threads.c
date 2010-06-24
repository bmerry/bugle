/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2009  Bruce Merry
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* Validate the API porting layer threading functions */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <bugle/memory.h>
#include <bugle/bool.h>
#include "platform/threads.h"
#include "test.h"

/* Number of threads to use in stress tests */
#define STRESS_THREADS 50

static unsigned int threads_create_child(void *arg)
{
    return *(unsigned int *) arg;
}

/* Checks that a child thread actually runs */
static void threads_create(void)
{
    unsigned int value = 17;
    unsigned int retval = 0;
    bugle_thread_handle thread;
    TEST_ASSERT(bugle_thread_create(&thread, threads_create_child, &value) == 0);
    TEST_ASSERT(bugle_thread_join(thread, &retval) == 0);
    TEST_ASSERT(retval == value);
}

typedef struct
{
    bugle_thread_id child_id;
} threads_self_struct;

static unsigned int threads_self_child(void *arg)
{
    *(bugle_thread_id *) arg = bugle_thread_self();
    return 0;
}

/* Checks that a child has different thread ID to parent */
static void threads_self(void)
{
    bugle_thread_id parent_id = bugle_thread_self();
    bugle_thread_id child_id = parent_id;
    bugle_thread_handle child;

    TEST_ASSERT(bugle_thread_equal(parent_id, child_id));

    TEST_ASSERT(bugle_thread_create(&child, threads_self_child, &parent_id) == 0);
    TEST_ASSERT(bugle_thread_join(child, NULL) == 0);
    TEST_ASSERT(bugle_thread_equal(parent_id, parent_id));
    TEST_ASSERT(bugle_thread_equal(child_id, child_id));
    TEST_ASSERT(!bugle_thread_equal(parent_id, child_id));
    TEST_ASSERT(!bugle_thread_equal(child_id, parent_id));
}

typedef struct
{
    int counter;    /* Incremented by each thread multiple times */
    int steps;      /* Number of times each thread should increment it */
    bugle_thread_lock_t lock; /* Lock to be held while incrementing counter */
} threads_lock_struct;

static unsigned int threads_lock_child(void *arg)
{
    threads_lock_struct *s = (threads_lock_struct *) arg;
    int i;

    for (i = 0; i < s->steps; i++)
    {
        volatile int tmp;

        bugle_thread_lock_lock(&s->lock);
        tmp = s->counter + 1;
        s->counter = tmp;
        bugle_thread_lock_unlock(&s->lock);
    }
    return 0;
}

static void threads_lock(void)
{
    int i;
    threads_lock_struct s;
    bugle_thread_handle children[STRESS_THREADS];

    s.counter = 0;
    s.steps = 100;
    bugle_thread_lock_init(&s.lock);
    for (i = 0; i < STRESS_THREADS; i++)
        TEST_ASSERT(bugle_thread_create(&children[i], threads_lock_child, &s) == 0);

    for (i = 0; i < STRESS_THREADS; i++)
        TEST_ASSERT(bugle_thread_join(children[i], NULL) == 0);

    /* Check that the locks really made the increments atomic */
    TEST_ASSERT(s.counter == STRESS_THREADS * s.steps);
}

/* Basic test of read-write locks - writers change states and readers check
 * that they only observe the states between locks.
 */
typedef struct
{
    int steps;     /* Number of locks held in each thread */
    int substeps;
    volatile int counter;
    bugle_thread_rwlock_t lock;
} threads_rwlock_struct;

unsigned int threads_rwlock_writer(void *arg)
{
    threads_rwlock_struct *s = (threads_rwlock_struct *) arg;
    int i, j;

    for (i = 0; i < s->steps; i++)
    {
        bugle_thread_rwlock_wrlock(&s->lock);
        for (j = 0; j < s->substeps; j++)
            s->counter++;
        bugle_thread_rwlock_unlock(&s->lock);
    }
    return 0;
}

/* Returns the number of bad observations */
unsigned int threads_rwlock_reader(void *arg)
{
    threads_rwlock_struct *s = (threads_rwlock_struct *) arg;
    int i;
    unsigned int bad = 0;

    for (i = 0; i < s->steps; i++)
    {
        bugle_thread_rwlock_rdlock(&s->lock);
        if (s->counter % s->substeps != 0)
            bad++;
        bugle_thread_rwlock_unlock(&s->lock);
    }
    return bad;
}

static void threads_rwlock(void)
{
    threads_rwlock_struct s;
    bugle_thread_handle readers[STRESS_THREADS];
    bugle_thread_handle writers[STRESS_THREADS];
    int i;

    s.steps = 100;
    s.substeps = 100;
    s.counter = 0;
    bugle_thread_rwlock_init(&s.lock);
    for (i = 0; i < STRESS_THREADS; i++)
    {
        TEST_ASSERT(bugle_thread_create(&writers[i], threads_rwlock_writer, &s) == 0);
        TEST_ASSERT(bugle_thread_create(&readers[i], threads_rwlock_reader, &s) == 0);
    }

    for (i = 0; i < STRESS_THREADS; i++)
        TEST_ASSERT(bugle_thread_join(writers[i], NULL) == 0);
    TEST_ASSERT(s.counter == s.steps * s.substeps * STRESS_THREADS);

    for (i = 0; i < STRESS_THREADS; i++)
    {
        unsigned int bad = 1;
        TEST_ASSERT(bugle_thread_join(readers[i], &bad) == 0);
        TEST_ASSERT(bad == 0);
    }
}

typedef struct
{
    size_t qsize;
    size_t head;
    size_t tail;
    int *q;
    bugle_thread_sem_t data_sem;
    bugle_thread_sem_t space_sem;
} threads_sem_struct;

static void threads_sem_enqueue(threads_sem_struct *s, int value)
{
    TEST_ASSERT(bugle_thread_sem_wait(&s->space_sem) == 0);
    s->q[s->tail] = value;
    if (++s->tail == s->qsize) s->tail = 0;
    TEST_ASSERT(bugle_thread_sem_post(&s->data_sem) == 0);
}

static int threads_sem_dequeue(threads_sem_struct *s)
{
    int value;
    TEST_ASSERT(bugle_thread_sem_wait(&s->data_sem) == 0);
    value = s->q[s->head];
    s->q[s->head] = -1;     /* Poison the value for testing */
    if (++s->head == s->qsize) s->head = 0;
    TEST_ASSERT(bugle_thread_sem_post(&s->space_sem) == 0);
    return value;
}

/* Returns the number of items produced (excluding the terminator) */
static unsigned int threads_sem_producer(void *arg)
{
    const unsigned int produce = 1000;
    unsigned int i;
    threads_sem_struct *s = (threads_sem_struct *) arg;

    for (i = 0; i < produce; i++)
        threads_sem_enqueue(s, 1);
    threads_sem_enqueue(s, 0);
    return produce;
}

/* Returns the number of items consumed (excluding the terminator) */
static unsigned int threads_sem_consumer(void *arg)
{
    unsigned int consume = 0;
    threads_sem_struct *s = (threads_sem_struct *) arg;

    while (BUGLE_TRUE)
    {
        int value = threads_sem_dequeue(s);
        if (value == 0)
            return consume;
        TEST_ASSERT(value == 1);
        consume++;
    }
}

/* Test semaphores by running a single-producer/single-consumer circule queue,
 * and checking that there are no bad values and that the number of items
 * produced equals the number consumed
 */
void threads_sem(void)
{
    threads_sem_struct s;
    bugle_thread_handle consumer, producer;
    unsigned int produced, consumed;

    s.qsize = 16;
    s.q = BUGLE_NMALLOC(s.qsize, int);

    TEST_ASSERT(bugle_thread_sem_init(&s.data_sem, 0) == 0);
    TEST_ASSERT(bugle_thread_sem_init(&s.space_sem, s.qsize) == 0);
    s.head = 0;
    s.tail = 0;

    TEST_ASSERT(bugle_thread_create(&consumer, threads_sem_consumer, &s) == 0);
    TEST_ASSERT(bugle_thread_create(&producer, threads_sem_producer, &s) == 0);

    TEST_ASSERT(bugle_thread_join(consumer, &consumed) == 0);
    TEST_ASSERT(bugle_thread_join(producer, &produced) == 0);

    TEST_ASSERT(consumed == produced);

    TEST_ASSERT(bugle_thread_sem_destroy(&s.data_sem) == 0);
    TEST_ASSERT(bugle_thread_sem_destroy(&s.space_sem) == 0);
    bugle_free(s.q);
}

test_status threads_suite(void)
{
    threads_create();
    threads_self();
    threads_lock();
    threads_rwlock();
    threads_sem();
    return TEST_RAN;
}
