#include "uthread.h"
#include "TCB.h"
#include <cassert>
#include <deque>
#include <ostream>
#include <ucontext.h>

using namespace std;

// Finished queue entry type
typedef struct finished_queue_entry
{
	TCB *tcb;	  // Pointer to TCB
	void *result; // Pointer to thread result (output)
} finished_queue_entry_t;
<<<<<<< HEAD
=======
static sigset_t mask;
static int next_tid = 1;
>>>>>>> 19b1dcee54fc706fce22c5a8616f4342729e5f32

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

TCB *curr; // current TCB

int total_quantums = 0;


// Interrupt Management --------------------------------------------------------
static sigset_t mask;

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
static deque<TCB *> ready_queue;
static deque<join_queue_entry_t> join_queue;
static deque<finished_queue_entry_t> finished_queue;

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

static void switchThreads() {
    // Error check for ready queue
    if (ready_queue.empty()) {
        if (curr && curr->getState() == FINISH) {
            cerr << "All threads completed" << endl;
            std::cout << "[DEBUG] Adding TID " << curr->getId() << " to finish queue." << std::endl;
            void* retval = nullptr;
            finished_queue_entry_t entry;
            entry.tcb = curr;
            entry.result = retval;
            finished_queue.push_back(entry);
            enableInterrupts();
            exit(EXIT_SUCCESS);
        } else {
            cerr << "Deadlock: No runnable threads" << endl;
            enableInterrupts();
            exit(EXIT_FAILURE);
        }
    }
    if (getcontext(&curr->_context) == -1) {
        std::cerr << "Error saving context for previous thread" << std::endl;
        exit(EXIT_FAILURE);
    }
    TCB* prev_thread = curr; // Save the current thread for swap

    if(ready_queue.empty()){
        std::cerr << "Error: next_thread is NULL in switchThreads()" << std::endl;
        exit(1);
    }
    curr = popFromReadyQueue();
    //modify prev here:
    if (prev_thread != nullptr) {
        // Only re-queue threads that are not finished
        if (prev_thread->getState() != FINISH) {
            prev_thread->setState(READY);
            addToReadyQueue(prev_thread);  // Add to ready queue after finishing
        }
    }
    else{
        std::cerr << "Warning: prev_thread is NULL, using setcontext instead of swapcontext." << std::endl;
    }
    /*if (!curr) {
        std::cerr << "Error: next_thread is NULL in switchThreads()" << std::endl;
        exit(1);
    }*/
    
    curr->setState(RUNNING);
    if (setcontext(&curr->_context) == -1) {
        std::cerr << "Error restoring context" << std::endl;
        exit(EXIT_FAILURE);
    }
    std::cout << "Switching to current_thread: " << curr->getId() << std::endl;
    /*if (getcontext(&curr->_context) == -1) {
        std::cerr << "Error switching to current thread" << std::endl;
        exit(EXIT_FAILURE);
    }*/

    std::cout << "yield done, current thread is " << curr->getId() 
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
    curr = main_tcb;
    std::cout << "[DEBUG] Main thread initialized with TID 0." << std::endl;
    return 0;
}

<<<<<<< HEAD
static int next_tid = 1;
=======

>>>>>>> 19b1dcee54fc706fce22c5a8616f4342729e5f32

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
    std::cout << "[DEBUG] Created current TID = " << tid << std::endl;
	std::cout << "[DEBUG] uthread_create: start_routine = " << (void*)start_routine 
	<< ", arg = " << arg << std::endl;
    enableInterrupts();
    return tid;
}

// In order to synchronize thread completions
// a thread can call uthread_join to block 
// until the specified thread has terminated. 
int uthread_join(int tid, void **retval)
{
	disableInterrupts();
    
    // check wheather the thread is finished
    // If the thread specified by tid is already terminated, just return
    for (finished_queue_entry_t& entry : finished_queue) {
        if (entry.tcb->getId() == tid) {
			//if (retval) *retval = entry.result;
            enableInterrupts();
            return 0;
        }
    }

	// If the thread specified by tid is still running
    // block until it terminates
    join_queue_entry_t entry = {curr, tid};
    join_queue.push_back(entry);
    curr->setState(BLOCK);
    enableInterrupts();
    switchThreads();
	
    // Set *retval to be the result of thread if retval != nullptr
    disableInterrupts();
    if (retval) {
        for (finished_queue_entry_t& entry : finished_queue) {
            if (entry.tcb->getId() == tid) {            
                *retval = entry.result; // 将线程的返回值存储到 retval
            }
            break;
        }
    }
    enableInterrupts();

    return 0;
}

int uthread_yield(void)
{
    disableInterrupts();

    if (!curr) {
        std::cerr << "Yield Error: No running thread" << std::endl;
        enableInterrupts();
        return -1;
    }

    std::cout << "[DEBUG] Entering uthread_yield function." << std::endl;
    std::cout << "[DEBUG] Yielding current thread TID: " << curr->getId() << std::endl;

    switchThreads();  

    enableInterrupts();
    return 1;
}

void uthread_exit(void *retval) 
{
    disableInterrupts();
    
    // Set the thread state to FINISH (terminated)
    curr->setState(State::FINISH);
    std::cout << "[DEBUG] Adding TID " << curr->getId() << " to finish queue." << std::endl;
    finished_queue_entry_t entry;
    entry.tcb = curr;
    entry.result = retval;
    finished_queue.push_back(entry);

    // Wake up any threads waiting for this one
    for (auto it = join_queue.begin(); it != join_queue.end();) {
        if (it->waiting_for_tid == curr->getId()) {
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
    return curr->getId( );
}

int uthread_get_total_quantums()
{
    return total_quantums;
}

int uthread_get_quantums( int tid )
{
    return -1;
}