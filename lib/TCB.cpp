#include "TCB.h"

#include <cstdint>



TCB::TCB(int tid, Priority pr, void *(*start_routine)(void *), void *arg, State state) {
    std::cout << "[DEBUG] TCB Constructor - TID: " << tid 
              << ", start_routine: " << (void*)start_routine 
              << ", arg: " << arg << std::endl;

    // Initialize context first
    if (getcontext(&_context) == -1) {
        std::cerr << "[ERROR] getcontext failed: " << std::endl;
        throw std::runtime_error("Failed to get context");
    }

    _tid = tid;
    _quantum = 0;
    _state = state;
    _stack = nullptr;

    // Only set up stack and context for non-main threads
    if (tid > 0) {
        // Allocate stack with 16-byte alignment
        _stack = new char[STACK_SIZE];
        if (!_stack) {
            std::cerr << "[ERROR] Stack allocation failed" << std::endl;
            throw std::runtime_error("Failed to allocate stack");
        }

        // Verify stack alignment
        if (((uintptr_t)_stack & 15) != 0) {
            std::cerr << "[ERROR] Stack not 16-byte aligned: " << _stack << std::endl;
            delete[] _stack;
            throw std::runtime_error("Stack alignment error");
        }

        std::cout << "[DEBUG] Stack allocated at " << _stack 
                  << " with size " << STACK_SIZE << std::endl;

        // Set up the context
        _context.uc_stack.ss_sp = _stack;  // No need for manual alignment
        _context.uc_stack.ss_size = STACK_SIZE;
        _context.uc_stack.ss_flags = 0;
        _context.uc_link = nullptr;

        // Clear signal mask
        sigemptyset(&_context.uc_sigmask);

        // Verify function pointer before makecontext
        if (!start_routine) {
            std::cerr << "[ERROR] Invalid start_routine pointer" << std::endl;
            delete[] _stack;
            throw std::runtime_error("Invalid function pointer");
        }

        // Create the context
        try {
            makecontext(&_context, (void (*)())stub, 2, start_routine, arg);
        } catch (...) {
            std::cerr << "[ERROR] makecontext failed" << std::endl;
            delete[] _stack;
            throw std::runtime_error("Failed to create context");
        }

        std::cout << "[DEBUG] Created context for TID: " << _tid << std::endl;
    }
}

TCB::~TCB()
{
    if (_stack)
    {
        delete[] _stack; 
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
    if (getcontext(&_context) == -1) {
        throw std::runtime_error("Failed to get context");
    }
    return 0;
}

void TCB::loadContext()
{
    if (setcontext(&_context) == -1) {
        std::cerr << "Error: setcontext failed" << std::endl;
        throw std::runtime_error("Failed to set context");
    }
}

