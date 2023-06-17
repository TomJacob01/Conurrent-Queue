#include "queue.h"
#include <threads.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <stdio.h>

typedef struct queue_node {
    void* val;
    struct queue_node *next;
} queue_node;

typedef struct queue {
    queue_node* head;
    queue_node* tail;
    atomic_size_t size;
    atomic_size_t visited;
} queue;

typedef struct thread_node {
    cnd_t  cv;
    struct thread_node *next;
    atomic_int age;
} thread_node;

typedef struct thread_queue {
    thread_node* head;
    thread_node* tail;
    atomic_size_t size;
} thread_queue;
// ============================ BODY ============================

// The queue we use and the queue to store threads inorder
queue mainq;
thread_queue thrdq;
bool is_init = 0;

// Mutex locks
mtx_t main_lock;
mtx_t thread_lock;

void initQueue(void) {
    if (is_init) {return;}
    is_init = 1;

    mainq.size = 0;
    mainq.visited = 0;
    mainq.head = NULL;
    mainq.tail = NULL; 

    thrdq.size = 0;
    thrdq.head = NULL;
    thrdq.tail = NULL;

    mtx_init(&main_lock, mtx_plain);
    mtx_init(&thread_lock, mtx_plain);
}

void destroyQueue(void) {
    if (!is_init) {return;}
    mtx_lock(&main_lock);
    mtx_lock(&thread_lock);
    is_init = 0;
    
    queue_node *curr = mainq.head, *next;
    while (curr != NULL) {
        next = curr->next;
        free(curr);
        curr = next;
    }

    thread_node *tmp = thrdq.head, *nxt;
    while (tmp != NULL) {
        nxt = tmp->next;
        free(tmp);
        tmp = nxt;
    }

    mtx_unlock(&thread_lock);
    mtx_unlock(&main_lock);

    mtx_destroy(&main_lock);
    mtx_destroy(&thread_lock);
}

size_t visited(void) { return mainq.visited; }
size_t size(void) { return mainq.size; }
size_t waiting(void) { return thrdq.size; }

void enqueue(void* item) {
    mtx_lock(&main_lock);

    queue_node *new_node = malloc(sizeof(queue_node));
    new_node -> val = item;
    new_node -> next = NULL;

    if (mainq.head == NULL) { mainq.head = new_node; }
    else { mainq.tail -> next = new_node; }
    mainq.tail = new_node;
    mainq.size++;

    if (waiting() > 0){ cnd_signal(&(thrdq.head ->cv)); }

    mtx_unlock(&main_lock);
}

bool tryDequeue(void** ptr) {
    if (mainq.size == 0 || waiting() > 0 ||
     (mtx_trylock(&main_lock) != thrd_success)) { 
        return false; 
    }

    queue_node *new_head = mainq.head -> next;
    *ptr = (void*)mainq.head->val;

    mainq.head -> next = NULL;
    free(mainq.head);

    mainq.head = new_head;
    mainq.visited++;
    mainq.size--;

    mtx_unlock(&main_lock);
    return true;
}

void* dequeue(void) {
    thread_node *thrd;
    queue_node *head = NULL;
    void *output;

    mtx_lock(&main_lock);
    // We need to freeze the thread, add to q
    if (mainq.size == 0 || thrdq.size > 0) {
        thrdq.size++;
        mtx_lock(&thread_lock);

        thrd = malloc(sizeof(thread_node));
        thrd->next = NULL;
        cnd_init(&(thrd->cv));

        // first in q
        if (thrdq.size == 1) {
            thrdq.head = thrd;
            thrdq.tail = thrd;
        }
        // not first in q
        else { 
            thrdq.tail ->next = thrd;
            thrdq.tail = thrd;
        }
        mtx_unlock(&thread_lock);

        // sleep until cond
        while (mainq.size == 0 || thrd != thrdq.head) { cnd_wait(&(thrd->cv), &main_lock); }


        thrdq.head = thrd ->next;
        thrdq.size--;
        if (thrdq.size == 0) { thrdq.head = NULL; thrdq.tail = NULL;}

        // time for dequeue
        cnd_destroy(&(thrd->cv));
        free(thrd);
        if (waiting() > 0 && size() > 0) { cnd_signal(&(thrdq.head->cv)); }
    }
    
    mainq.size--;
    mainq.visited++;
    head = mainq.head;
    mainq.head = head->next;
    if (head->next == NULL) { mainq.tail = NULL; }

    output = head->val;
    free(head);
    mtx_unlock(&main_lock);
    return output;
}
