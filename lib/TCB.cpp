#include "TCB.h"




TCB::TCB(int tid, Priority pr, void *(*start_routine)(void *), void *arg, State state)
{
    getcontext(&_context); // Initialize the thread context

    _context.uc_stack.ss_sp = malloc(STACK_SIZE);
    if (!_context.uc_stack.ss_sp)
    {
        std::cerr << "Failed to allocate stack memory\n";
        exit(1);
    }
    _context.uc_stack.ss_size = STACK_SIZE;
    _context.uc_stack.ss_flags = 0;
    _context.uc_link = nullptr;

    makecontext(&_context, (void (*)())stub, 2, start_routine, arg);
}

TCB::~TCB()
{
    if (_context.uc_stack.ss_sp)
    {
        free(_context.uc_stack.ss_sp);
    }
}

void TCB::setState(State state)
{
    _state = state;
}

State TCB::getState() const
{
    return _state;
}

int TCB::getId() const
{
    return _tid;
}

void TCB::increaseQuantum()
{
    _quantum++;
}

int TCB::getQuantum() const
{
    return _quantum;
}

int TCB::saveContext()
{
    return getcontext(&_context);
}

void TCB::loadContext()
{
    setcontext(&_context);
}
