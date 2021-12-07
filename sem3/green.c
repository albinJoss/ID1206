#include <ucontext.h>
#include <stdlib.h>
#include <assert.h>
#include <signal.h>
#include "green.h"

#define FALSE 0
#define TRUE 1

#define STACK_SIZE 4096

static ucontext_t main_cntx = {0};
static green_t main_green = {&main_cntx, NULL, NULL, NULL, NULL, FALSE};

static green_t *running = &main_green;

struct green_t *ready_queue = NULL;

static sigset_t block;

static void init() __attribute__((constructor));

void init()
{
    getcontext(&main_cntx);
}

void enqueue(green_t **list, green_t *thread)
{
    if(*list == NULL)
    {
        return NULL;
    }
    else
    {
        green_t *susp = *list;
        while(susp->next != NULL)
        {
            susp = susp->next;

        }
        susp->next = thread;
    }
}

void dequeue(green_t **list)
{
    if(*list == NULL)
    {
        return NULL;
    }
    else
    {
        green_t *thread = *list;
        *list = (*list)->next;
        thread->next = NULL;
        return NULL;
    }
}

void green_thread()
{
    green_t *this = running;

    void *result = (*this->fun)(this->arg);
    // place waiting (joining) thread in ready queue
    enqueue(this->join);

    // save result of execution
    this->retval = result;

    // we're a zombie
    this->zombie = TRUE;
    // find the next thread to run

    running = this->next;
    setcontext(next−>context);
}


int green_create(green_t *new, void *(*fun)(void *), void *arg)
{
    ucontext_t *cntx = (ucontext_t *)malloc(sizeof(ucontext_t));
    getcontext(cntx);

    void *stack = malloc(STACK_SIZE);

    cntx->uc_stack.ss_sp = stack;
    cntx->uc_stack.ss_size = STACK_SIZE;
    makecontext(cntx, green_thread, 0);

    new->context = cntx;
    new->fun = fun;
    new->arg = arg;
    new->next = NULL;
    new->join = NULL;
    new->retval = NULL;
    new->zombie = NULL;

    // add new to the ready queue
    //sigprocmask(SIG_BLOCK, &block, NULL);
    enqueue(new);
    //sigprocmask(SIG_UNBLOCK, &block, NULL);

    return 0;
}

int green_yield()
{
    green_t *susp = running;
    // add susp to ready queue

    // select the next thread for execution

    running = next;
    swapcontext(susp->context, next->conext);
}