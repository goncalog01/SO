#ifndef STACK_H
#define STACK_H

#define STACK_SIZE 50
#define FALSE 0
#define TRUE 1

typedef struct stack {
    int count;
    int size;
    int *inumbers;
} *Stack;

Stack STACKinit(int size);
int STACKpop(Stack stack);
void STACKpush(Stack stack, int inumber);
void STACKfree(Stack stack);
int STACKcontains(Stack stack, int inumber);

#endif /* STACK_H */