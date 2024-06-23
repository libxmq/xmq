/* libxmq - Copyright (C) 2023 Fredrik Öhrström (spdx: MIT)

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#ifndef BUILDING_XMQ

#include"stack.h"
#include<assert.h>
#include<string.h>

#endif

#ifdef STACK_MODULE

StackElement *find_element_above(Stack *s, StackElement *b);

Stack *new_stack()
{
    Stack *s = (Stack*)malloc(sizeof(Stack));
    memset(s, 0, sizeof(Stack));
    return s;
}

void free_stack(Stack *stack)
{
    if (!stack) return;

    StackElement *e = stack->top;
    while (e)
    {
        StackElement *next = e->below;
        free(e);
        e = next;
    }

    free(stack);
}

void push_stack(Stack *stack, void *data)
{
    assert(stack);
    StackElement *element = (StackElement*)malloc(sizeof(StackElement));
    memset(element, 0, sizeof(StackElement));
    element->data = data;

    if (stack->top == NULL) {
        stack->top = stack->bottom = element;
    }
    else
    {
        element->below = stack->top;
        stack->top = element;
    }
    stack->size++;
}

void *pop_stack(Stack *stack)
{
    assert(stack);
    assert(stack->top);

    void *data = stack->top->data;
    StackElement *element = stack->top;
    stack->top = element->below;
    free(element);
    stack->size--;
    return data;
}

StackElement *find_element_above(Stack *s, StackElement *b)
{
    StackElement *e = s->top;

    for (;;)
    {
        if (!e) return NULL;
        if (e->below == b) return e;
        e = e->below;
    }

    return NULL;
}

void *rock_stack(Stack *stack)
{
    assert(stack);
    assert(stack->bottom);

    assert(stack->size > 0);

    if (stack->size == 1) return pop_stack(stack);

    StackElement *element = stack->bottom;
    void *data = element->data;
    StackElement *above = find_element_above(stack, element);
    assert(above);
    stack->bottom = above;
    above->below = NULL;
    free(element);
    stack->size--;

    return data;
}

#endif // STACK_MODULE
