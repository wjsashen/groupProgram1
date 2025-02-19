#include "TCB.h"




TCB::TCB(int tid, Priority pr, void *(*start_routine)(void *), void *arg, State state)
{
    getcontext(&_context); // Initialize the thread context
    _tid = tid;
    _quantum = 0;
    _state = state;
    if (tid > 0) {
        _stack = new char[STACK_SIZE];
        _context.uc_stack.ss_sp = _stack;
        _context.uc_stack.ss_size = STACK_SIZE;
        _context.uc_stack.ss_flags = 0;
        makecontext(&_context, (void (*)())stub, 2, start_routine, arg);
    }
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
