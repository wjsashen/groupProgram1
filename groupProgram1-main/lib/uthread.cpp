#include "uthread.h"
#include "TCB.h"
#include <cassert>
#include <deque>
#include <ostream>
#include <ucontext.h>

using namespace std;
TCB *curr;
// Finished queue entry type
typedef struct finished_queue_entry
{
	TCB *tcb;	  // Pointer to TCB
	void *result; // Pointer to thread result (output)
} finished_queue_entry_t;
static sigset_t mask;
// Join queue entry type
typedef struct join_queue_entry
{
	TCB *tcb;			 // Pointer to TCB
	int waiting_for_tid; // TID this thread is waiting on
} join_queue_entry_t;

// You will need to maintain structures to track the state of threads
// - uthread library functions refer to threads by their TID so you will want
//   to be able to access a TCB given a thread ID
// - Threads move between different states in their lifetime (READY, BLOCK,
//   FINISH). You will want to maintain separate "queues" (doesn't have to
//   be that data structure) to move TCBs between different thread queues.
//   Starter code for a ready queue is provided to you
// - Separate join and finished "queues" can also help when supporting joining.
//   Example join and finished queue entry types are provided above

// Queues
static deque<TCB *> ready_queue;
int total_quantums = 0;
// Interrupt Management --------------------------------------------------------

// Start a countdown timer to fire an interrupt
static void startInterruptTimer(int quantum_usecs)
{
	// TODO
	struct itimerval timer;

    // Set timer interval (repeating)
    timer.it_interval.tv_sec = quantum_usecs / 1000000;
    timer.it_interval.tv_usec = quantum_usecs % 1000000;

    // Set initial timer expiration
    timer.it_value.tv_sec = quantum_usecs / 1000000;
    timer.it_value.tv_usec = quantum_usecs % 1000000;

    if (setitimer(ITIMER_VIRTUAL, &timer, NULL) == -1)
    {
        perror("setitimer");
        exit(EXIT_FAILURE);
    }
}

// Block signals from firing timer interrupt
static void disableInterrupts()
{
	// TODO
    sigemptyset(&mask);
    sigaddset(&mask, SIGVTALRM);
    sigprocmask(SIG_BLOCK, &mask, nullptr);
}

// Unblock signals to re-enable timer interrupt
static void enableInterrupts()
{
	// TODO
    sigprocmask(SIG_UNBLOCK, &mask, nullptr);
}

// Queue Management ------------------------------------------------------------

// Add TCB to the back of the ready queue
void addToReadyQueue(TCB* tcb) {
    if (!tcb) {
        std::cerr << "[ERROR] Attempted to add NULL TCB to ready queue!" << std::endl;
        return;
    }

    std::cout << "[DEBUG] Adding TID " << tcb->getId() << " to ready queue." << std::endl;
    tcb->setState(READY);
    ready_queue.push_back(tcb);
}


// Removes and returns the first TCB on the ready queue
// NOTE: Assumes at least one thread on the ready queue
TCB *popFromReadyQueue()
{
	assert(!ready_queue.empty());
	std::cout << "[DEBUG] Ready queue contents before switch: ";
	for (auto t : ready_queue) {
		std::cout << "TID " << t->getId() << " ";
	}
	std::cout << std::endl;
	
	TCB *ready_queue_head = ready_queue.front();
	ready_queue.pop_front();
	return ready_queue_head;
}

// Removes the thread specified by the TID provided from the ready queue
// Returns 0 on success, and -1 on failure (thread not in ready queue)
int removeFromReadyQueue(int tid)
{
	for (deque<TCB *>::iterator iter = ready_queue.begin(); iter != ready_queue.end(); ++iter)
	{
		if (tid == (*iter)->getId())
		{
			ready_queue.erase(iter);
			return 0;
		}
	}

	// Thread not found
	return -1;
}

// Helper functions ------------------------------------------------------------
static TCB* current_thread = nullptr; // 当前运行线程
static deque<TCB*> finished_queue;    // 已完成线程队列
static deque<join_queue_entry_t> join_queue; // 等待队列
static void switchThreads() {
    // Error check for ready queue
    if (ready_queue.empty()) {
        if (current_thread && current_thread->getState() == FINISH) {
            cerr << "All threads completed" << endl;
            enableInterrupts();
            exit(EXIT_SUCCESS);
        } else {
            cerr << "Deadlock: No runnable threads" << endl;
            enableInterrupts();
            exit(EXIT_FAILURE);
        }
    }
    if (getcontext(&current_thread->_context) == -1) {
        std::cerr << "Error saving context for previous thread" << std::endl;
        exit(EXIT_FAILURE);
    }
    TCB* prev_thread = current_thread; // Save the current thread for swap


    current_thread = popFromReadyQueue();
    //modify prev here:
    if (prev_thread != nullptr) {
        // Only re-queue threads that are not finished
        if (prev_thread->getState() != FINISH) {
            prev_thread->setState(READY);
            addToReadyQueue(prev_thread);  // Add to ready queue after finishing
        }
    }
    if (!current_thread) {
        std::cerr << "Error: next_thread is NULL in switchThreads()" << std::endl;
        exit(1);
    }
    current_thread->setState(RUNNING);
    if (!prev_thread) {
        std::cerr << "Warning: prev_thread is NULL, using setcontext instead of swapcontext." << std::endl;
    }

    std::cout << "Switching to current_thread: " << current_thread->getId() << std::endl;
    if (getcontext(&current_thread->_context) == -1) {
        std::cerr << "Error switching to current thread" << std::endl;
        exit(EXIT_FAILURE);
    }

    std::cout << "yield done, current thread is " << current_thread->getId() 
              << " prev thread that yielded is " << prev_thread->getId() << std::endl;
}

// Library functions -----------------------------------------------------------

// The function comments provide an (incomplete) summary of what each library
// function must do

// Starting point for thread. Calls top-level thread function
void stub(void *(*start_routine)(void *), void *arg) {
    void* retval = start_routine( arg );
    uthread_exit( retval );
}





int uthread_init(int quantum_usecs)
{
    // Initialize any data structures
    // Setup timer interrupt and handler
    // Create a thread for the caller (main) thread
    // Inter check: set up alarm timer, but the handler does nothing right now, not critical
    //startInterruptTimer(quantum_usecs);
    // Create and initialize TCB for the main thread
    TCB *main_tcb = new TCB(0, Priority::ORANGE, nullptr, nullptr, State::RUNNING);
    // Set current thread to the main thread
    current_thread = main_tcb;
    std::cout << "[DEBUG] Main thread initialized with TID 0." << std::endl;
    return 0;
}


static int next_tid = 1;

int getNewTid() {
    return next_tid++;  
}
int uthread_create(void *(*start_routine)(void *), void *arg) {
    disableInterrupts();
    
    int tid = getNewTid(); 
    TCB *new_tcb = new TCB(tid, Priority::ORANGE, start_routine, arg, State::READY);
    if (!new_tcb) {
        return -1; // Memory allocation failed
    }

    // Make sure the function pointer is not null
    if (!start_routine) {
        std::cerr << "[ERROR] Attempted to create thread with null function pointer!" << std::endl;
        delete new_tcb;
        enableInterrupts();
        return -1;
    }

    addToReadyQueue(new_tcb);

    std::cout << "[DEBUG] Created new thread with TID " << new_tcb->getId() << std::endl;
	cout << "[DEBUG] uthread_create: start_routine = " << (void*)start_routine 
	<< ", arg = " << arg << endl;
    enableInterrupts();
    return tid;
}


int uthread_join(int tid, void **retval)
{
	// If the thread specified by tid is already terminated, just return
	// If the thread specified by tid is still running, block until it terminates
	// Set *retval to be the result of thread if retval != nullptr
	disableInterrupts();
    
    // 查找目标线程是否已完成
    for (auto& entry : finished_queue) {
        if (entry->getId() == tid) {
            //todo: ???
			//if (retval) *retval = entry.result;
            enableInterrupts();
            return 0;
        }
    }
    
    // 未完成则加入等待队列
    join_queue_entry_t entry = {current_thread, tid};
    join_queue.push_back(entry);
    current_thread->setState(BLOCK);
    
    enableInterrupts();
    
    // 主动让出CPU
    switchThreads();
    
    // 被唤醒后继续执行
    return 0;
}
int uthread_yield(void)
{
    disableInterrupts();

    std::cout << "[DEBUG] Entering uthread_yield function." << std::endl;
    std::cout << "[DEBUG] Yielding current thread TID: " << current_thread->getId() << std::endl;

    switchThreads();  

    enableInterrupts();
    return 1;
}

void uthread_exit(void *retval) 
{
    disableInterrupts();
    
    // Set the thread state to FINISH (terminated)
    current_thread->setState(State::FINISH);

    finished_queue.push_back(current_thread);

    // Wake up any threads waiting for this one
    for (auto it = join_queue.begin(); it != join_queue.end();) {
        if (it->waiting_for_tid == current_thread->getId()) {
            addToReadyQueue(it->tcb);
            it = join_queue.erase(it);
        } else {
            ++it;
        }
    }

    switchThreads(); 
}


int uthread_suspend(int tid)
{
	// Move the thread specified by tid from whatever state it is
	// in to the block queue
    return 1;
}

int uthread_resume(int tid)
{
	// Move the thread specified by tid back to the ready queue
    return tid;
}

int uthread_once(uthread_once_t *once_control, void (*init_routine)(void))
{
	// Use the once_control structure to determine whether or not to execute
	// the init_routine
	// Pay attention to what needs to be accessed and modified in a critical region
	// (critical meaning interrupts disabled)
    return 0;
}

int uthread_self()
{
    return current_thread->getId( );
}

int uthread_get_total_quantums()
{
    return total_quantums;
}

int uthread_get_quantums( int tid )
{
    return -1;
}