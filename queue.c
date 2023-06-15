#include <threads.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>
#include "queue.h"

typedef struct list_node {
    void* val;
    struct list_node *next;
} list_node;

typedef struct linked_list {
    list_node* head;
    list_node* tail;
} linked_list;

int main(int argc,char **argsv) {
	 printf("heyyyyy");
    linked_list l1 = {NULL, NULL};  // Initialize with NULL pointers
    list_node node = {NULL, NULL};  // Initialize with NULL pointers

    node.val = (void*)1;
    node.next = NULL;
    l1.head = &node;
    l1.tail = &node;

    printf("The list head is: %d\n", *(int*)l1.head->val);

    return 0;
}
