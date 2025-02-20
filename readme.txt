Feb 19 Log:
We made some progress in the past week. 
Submit it now for the intermediate check(since it could yield from thread0 to thread1).

Our program running log after the WED morning oh is like this(got rid of main context code):
[DEBUG] TCB Constructor - TID: 0, start_routine: 0, arg: 0
[DEBUG] Main thread initialized with TID 0.
[DEBUG] TCB Constructor - TID: 1, start_routine: 0x5ec79d1e765e, arg: 0x7ffcdcb55680
[DEBUG] Stack allocated at  with size 2113536
[DEBUG] Created context for TID: 1
[DEBUG] Adding TID 1 to ready queue.
[DEBUG] Created new thread with TID 1
[DEBUG] uthread_create: start_routine = 0x5ec79d1e765e, arg = 0x7ffcdcb55680
[DEBUG] TCB Constructor - TID: 2, start_routine: 0x5ec79d1e765e, arg: 0x7ffcdcb55680
[DEBUG] Stack allocated at  with size 2113536
[DEBUG] Created context for TID: 2
[DEBUG] Adding TID 2 to ready queue.
[DEBUG] Created new thread with TID 2
[DEBUG] uthread_create: start_routine = 0x5ec79d1e765e, arg = 0x7ffcdcb55680
[DEBUG] TCB Constructor - TID: 3, start_routine: 0x5ec79d1e765e, arg: 0x7ffcdcb55680
[DEBUG] Stack allocated at  with size 2113536
[DEBUG] Created context for TID: 3
[DEBUG] Adding TID 3 to ready queue.
[DEBUG] Created new thread with TID 3
[DEBUG] uthread_create: start_routine = 0x5ec79d1e765e, arg = 0x7ffcdcb55680
[DEBUG] TCB Constructor - TID: 4, start_routine: 0x5ec79d1e765e, arg: 0x7ffcdcb55680
[DEBUG] Stack allocated at  with size 2113536
[DEBUG] Created context for TID: 4
[DEBUG] Adding TID 4 to ready queue.
[DEBUG] Created new thread with TID 4
[DEBUG] uthread_create: start_routine = 0x5ec79d1e765e, arg = 0x7ffcdcb55680
[DEBUG] TCB Constructor - TID: 5, start_routine: 0x5ec79d1e765e, arg: 0x7ffcdcb55680
[DEBUG] Stack allocated at  with size 2113536
[DEBUG] Created context for TID: 5
[DEBUG] Adding TID 5 to ready queue.
[DEBUG] Created new thread with TID 5
[DEBUG] uthread_create: start_routine = 0x5ec79d1e765e, arg = 0x7ffcdcb55680
[DEBUG] TCB Constructor - TID: 6, start_routine: 0x5ec79d1e765e, arg: 0x7ffcdcb55680
[DEBUG] Stack allocated at  with size 2113536
[DEBUG] Created context for TID: 6
[DEBUG] Adding TID 6 to ready queue.
[DEBUG] Created new thread with TID 6
[DEBUG] uthread_create: start_routine = 0x5ec79d1e765e, arg = 0x7ffcdcb55680
[DEBUG] TCB Constructor - TID: 7, start_routine: 0x5ec79d1e765e, arg: 0x7ffcdcb55680
[DEBUG] Stack allocated at  with size 2113536
[DEBUG] Created context for TID: 7
[DEBUG] Adding TID 7 to ready queue.
[DEBUG] Created new thread with TID 7
[DEBUG] uthread_create: start_routine = 0x5ec79d1e765e, arg = 0x7ffcdcb55680
[DEBUG] TCB Constructor - TID: 8, start_routine: 0x5ec79d1e765e, arg: 0x7ffcdcb55680
[DEBUG] Stack allocated at  with size 2113536
[DEBUG] Created context for TID: 8
[DEBUG] Adding TID 8 to ready queue.
[DEBUG] Created new thread with TID 8
[DEBUG] uthread_create: start_routine = 0x5ec79d1e765e, arg = 0x7ffcdcb55680
[DEBUG] Entering uthread_yield function.
[DEBUG] Yielding current thread TID: 0
[DEBUG] Ready queue contents before switch: TID 1 TID 2 TID 3 TID 4 TID 5 TID 6 TID 7 TID 8 
[DEBUG] Adding TID 0 to ready queue.
yield done current thread is 1 prev thread that yield is 0
Illegal instruction (core dumped)

I gdb walked thru the running, the trace is like:
(gdb) p next_thread
$1 = (TCB *) 0x555555574200
(gdb) n

Program received signal SIGILL, Illegal instruction.
0x0000555555557c6d in uthread_yield () at lib/uthread.cpp:277
277         switchThreads();  
(gdb) info register
rax            0x0                 0
rbx            0x555555560040      93824992280640
rcx            0x7ffff78e29db      140737346677211
rdx            0x0                 0
rsi            0x7fffffffd310      140737488343824
rdi            0x1                 1
rbp            0x7fffffffd3a0      0x7fffffffd3a0
rsp            0x7fffffffd390      0x7fffffffd390
r8             0xcccccccccccccccd  -3689348814741910323
r9             0x7fffffffd230      140737488343600
r10            0x1                 1
r11            0x202               514
r12            0x3                 3
r13            0x0                 0
r14            0x55555555fc38      93824992279608
--Type <RET> for more, q to quit, c to continue without paging--
r15            0x7ffff7ffd000      140737354125312
rip            0x555555557c6d      0x555555557c6d <uthread_yield()+137>
eflags         0x10246             [ PF ZF IF RF ]
cs             0x33                51
ss             0x2b                43
ds             0x0                 0
es             0x0                 0
fs             0x0                 0
gs             0x0                 0
fs_base        0x7ffff7e86740      140737352591168
gs_base        0x0                 0
(gdb) 
(gdb) p current_thread
$2 = (TCB *) 0x555555574200

According to moring office hour, we modify the save/load ->get/set context
got a new issue:
Switching to current_thread: 1
Switching to current_thread: 1
Switching to current_thread: 1
Switching to current_thread: 1
it keep printing the same message.

We are still fixing this, already sent email to prof for help. 