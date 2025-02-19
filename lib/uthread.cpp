#include "uthread.h"
#include "TCB.h"
#include <cassert>
#include <deque>

using namespace std;
TCB *curr;
// Finished queue entry type
typedef struct finished_queue_entry
{
	TCB *tcb;	  // Pointer to TCB
	void *result; // Pointer to thread result (output)
} finished_queue_entry_t;

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
}

// Unblock signals to re-enable timer interrupt
static void enableInterrupts()
{
	// TODO
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
    // ==================== 保存当前上下文 ====================
    if (current_thread != nullptr) {
        // Save the context of the current thread using getcontext
        static ucontext_t cont[2];  // Array for two threads' contexts
        static int currentThread = 0;  // Global variable for the current thread

        // Check if currentThread is within bounds
        if (currentThread < 0 || currentThread >= 2) {
            std::cerr << "ERROR: currentThread out of bounds" << std::endl;
            exit(1);
        }

        // Get the current context and store it in cont[currentThread]
        if (getcontext(&cont[currentThread]) == -1) {
            std::cerr << "ERROR: getcontext() failed!" << std::endl;
            exit(1);
        }

        std::cout << "SWITCH: currentThread = " << currentThread << std::endl;

        // Only re-queue threads that are not finished
        if (current_thread->getState() != FINISH) {
            current_thread->setState(READY);
            addToReadyQueue(current_thread);
        }
    }

    // ==================== 清理完成线程 ====================
    auto finished_iter = finished_queue.begin();
    while (finished_iter != finished_queue.end()) {
        TCB* finished_tcb = *finished_iter;

        // Wake up any joiners waiting for this thread
        auto join_iter = join_queue.begin();
        while (join_iter != join_queue.end()) {
            if (join_iter->waiting_for_tid == finished_tcb->getId()) {
                addToReadyQueue(join_iter->tcb);
                join_iter = join_queue.erase(join_iter);
            } else {
                ++join_iter;
            }
        }

        // Free resources of finished thread
        delete finished_tcb;
        finished_iter = finished_queue.erase(finished_iter);
    }

    // ==================== 选择新线程 ====================
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

    // Get the next thread from ready queue
    TCB* next_thread = ready_queue.front();
    if (!next_thread) {
        std::cerr << "Error: next_thread is NULL in switchThreads()" << std::endl;
        exit(1);
    }

    ready_queue.pop_front();
    std::cout << "[DEBUG] Switching to thread " << next_thread->getId() << std::endl;

    // Update the state of the new thread to RUNNING
    next_thread->setState(RUNNING);
    TCB* prev_thread = current_thread;

    // Ensure prev_thread is not null before accessing
    if (!prev_thread) {
        std::cerr << "Warning: prev_thread is NULL, using setcontext instead of swapcontext." << std::endl;
    }
    current_thread = next_thread;

    // ==================== 执行上下文切换 ====================
    static ucontext_t cont[2];  // Array for two threads' contexts
    static int currentThread = 0;  // Global variable for current thread

    // Initialize the context (this step is crucial)
    if (getcontext(&cont[currentThread]) == -1) {
        std::cerr << "ERROR: getcontext() failed!" << std::endl;
        exit(1);
    }

    std::cout << "SWITCH: currentThread = " << currentThread << std::endl;

    // Ensure we're not calling getcontext() after the first return
    volatile int flag = 0;  // Local flag variable for each thread
    if (flag == 1) {
        return;  // Resume the current thread
    }

    // First time returning from getcontext (switching threads)
    flag = 1;
    currentThread = 1 - currentThread;  // Switch to the other thread

    // Debug the thread switching information
    std::cout << "Switching from thread " << prev_thread->getId() 
         << " to thread " << current_thread->getId() << std::endl;

    std::cout << "Prev thread state: " << prev_thread->getState() << std::endl;
    std::cout << "Next thread state: " << current_thread->getState() << std::endl;
    std::cout << "setcontext from " << prev_thread << " to " << current_thread << std::endl;

    // Now switch to the next thread using setcontext
    if (setcontext(&cont[currentThread]) == -1) {
        std::cerr << "ERROR: setcontext() failed!" << std::endl;
        exit(1);
    }
    std::cout << "set done" << std::endl;
}



// Library functions -----------------------------------------------------------

// The function comments provide an (incomplete) summary of what each library
// function must do

// Starting point for thread. Calls top-level thread function
void stub(void *(*start_routine)(void *), void *arg) {
    try {
        std::cout << "[DEBUG] Entering stub: function ptr = " << (void*)start_routine 
                  << ", arg ptr = " << arg << std::endl;
        
        // Validate start_routine
        if (!start_routine) {
            std::cerr << "[ERROR] start_routine is NULL!" << std::endl;
            uthread_exit(nullptr);
            return;
        }

        // Print the address of start_routine
        std::cout << "[DEBUG] Calling start_routine at address: " << (void*)start_routine << std::endl;

        // Run the thread function inside try-catch
        void* result = nullptr;
        try {
			std::cout << "[DEBUG] Function pointer address: " << (void*)start_routine << std::endl;

            result = start_routine(arg); // This is where the crash might happen
            std::cout << "[DEBUG] Got the result here successfully: " << result << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[ERROR] Thread function threw exception: " << e.what() << std::endl;
            uthread_exit(nullptr);
            return;
        } catch (...) {
            std::cerr << "[ERROR] Thread function threw unknown exception" << std::endl;
            uthread_exit(nullptr);
            return;
        }

        std::cout << "[DEBUG] Thread function completed successfully, result = " << result << std::endl;
        uthread_exit(result);  // Pass the result to uthread_exit
    } catch (...) {
        std::cerr << "[FATAL] Exception in stub function" << std::endl;
        uthread_exit(nullptr);
    }
}





int uthread_init(int quantum_usecs)
{
    // Initialize any data structures
    // Setup timer interrupt and handler
    // Create a thread for the caller (main) thread
    // Inter check: set up alarm timer, but the handler does nothing right now, not critical
    startInterruptTimer(quantum_usecs);

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

    switchThreads();  // This will already add the current thread if needed

    enableInterrupts();
}

void uthread_exit(void *retval) 
{
    disableInterrupts();
    
    // Set the thread state to FINISH (terminated)
    current_thread->setState(State::FINISH);

    // Perform cleanup (if necessary)
    // current_thread->setResult(retval);


    enableInterrupts();
}


int uthread_suspend(int tid)
{
	// Move the thread specified by tid from whatever state it is
	// in to the block queue
}

int uthread_resume(int tid)
{
	// Move the thread specified by tid back to the ready queue
}

int uthread_once(uthread_once_t *once_control, void (*init_routine)(void))
{
	// Use the once_control structure to determine whether or not to execute
	// the init_routine
	// Pay attention to what needs to be accessed and modified in a critical region
	// (critical meaning interrupts disabled)
}

int uthread_self()
{
	// TODO
}

int uthread_get_total_quantums()
{
	// TODO
}

int uthread_get_quantums(int tid)
{
	// TODO
}
