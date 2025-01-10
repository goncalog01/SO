#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "stack.h"
#include "fs/state.h"

/*
 * Creates a stack with the size passed as input
 * Input:
 *   - size: the size of the stack
 * Returns: a pointer to the stack
 */
Stack STACKinit(int size) {
    Stack stack = (Stack)malloc(sizeof(struct stack));

    if (!stack) {
        fprintf(stderr, "Error: inumbers stack allocation failed\n");
        exit(EXIT_FAILURE);
    }

    stack->inumbers = (int*)malloc(size * sizeof(int));

    if (stack->inumbers == NULL) {
        free(stack);
        fprintf(stderr, "Error: inumbers stack allocation failed\n");
        exit(EXIT_FAILURE);
    }

    memset(stack->inumbers, 0, size);
    stack->count = 0;
    stack->size = size;
    
    return stack;
}

/*
 * Adds an inumber to the stack
 * Inputs:
 *   - stack
 *   - inumber
 */
void STACKpush(Stack stack, int inumber) {
    if (stack->count == stack->size) {
        stack->inumbers = realloc(stack, stack->size * 2);
        if (!stack->inumbers) {
            free(stack);
            fprintf(stderr, "Error: inumbers stack reallocation failed\n");
            exit(EXIT_FAILURE);
        }
        stack->size *= 2;
        STACKpush(stack, inumber);
    }
    else
        stack->inumbers[stack->count++] = inumber;
}

/*
 * Removes an inumber to the stack
 * Inputs:
 *   - stack
 * Returns:
 *   - the removed inumber
 *   - FAIL: if the stack is empty
 */
int STACKpop(Stack stack) {
    return stack->count > 0 ? stack->inumbers[--stack->count] : FAIL;
}

/*
 * Frees all the memory associated with the stack
 * Inputs:
 *   - stack
 */
void STACKfree(Stack stack) {
    free(stack->inumbers);
    free(stack);
}

/*
 * Checks if the inumber is in the stack
 * Inputs:
 *   - stack
 *   - inumber
 * Returns: TRUE or FALSE
 */
int STACKcontains(Stack stack, int inumber) {
    for (int i = 0; i < stack->count; i++)
        if (stack->inumbers[i] == inumber)
            return TRUE;
    return FALSE;
}