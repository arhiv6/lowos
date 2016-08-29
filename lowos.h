#pragma once
#ifndef LOWOS_H
#define LOWOS_H

#include <setjmp.h>
#include <stdbool.h>
#include <inttypes.h>

#define MAX_TASKS 20

// =================================================================================================
// Data types:

typedef enum { NOINIT = 0, ACTIVE, DELAYED, WAITING, WAIT_TIMEOUT } TaskState; // Task states
typedef enum { TRIGGERED = 0, NOTRIGGERED } Event;                             // Mutex states
typedef enum { UNLOCK = 0, LOCK } Mutex;                                        // Event states

typedef struct                          // структура с описанием задачи
{
    void (*pFunc)();                    // указатель на функцию задачи
    uint32_t delay;                     // задержка до следующего (или первого) запуска
    jmp_buf context;                    // контекст задачи
    TaskState state;                   // состояние задачи
    uint8_t *lockObject;                // указатель на event задачи
} _task;

extern jmp_buf _os_context;              // контекст системы
extern _task taskArray[MAX_TASKS];       // очередь задач
extern volatile uint8_t currentTask;     // текущая задача

// =================================================================================================
// Платформено-зависимые особенности:

// Переопределение функции setjmp, необходимо, чтобы компилятор не ругался на его встраивание:
#ifdef __linux
extern int _os_setjmp(jmp_buf env) __asm("_setjmp");
#else //#elif __arm__
extern int _os_setjmp(jmp_buf env) __asm("setjmp");
#endif

// Указание компилятору об обязательном встраивание функций:
#ifdef __GNUC__
#define _ALWAYS_INLINE static inline __attribute__((always_inline))
#elif __IAR_SYSTEMS_ICC__
#define _ALWAYS_INLINE _Pragma("inline=forced") inline
#endif

// =================================================================================================
// Прототипы функций

void os_initOS(void);
void os_loop(void);
void os_initTask(void (*taskFunc)(void));

// =================================================================================================

// Установка точки возврата и передача управления диспетчеру. Пример: os_yield();
_ALWAYS_INLINE void os_yield(void)
{
    _os_setjmp(taskArray[currentTask].context) ? 0 : longjmp(_os_context, 1);
}

// Приостановка потока на заданное время. Пример: os_delay(12);
_ALWAYS_INLINE void os_delay(uint32_t time)
{
    taskArray[currentTask].delay = time;
    taskArray[currentTask].state = DELAYED;
    os_yield();
}

// Приостановка потока до тех пор, пока не выполнится условие (при этом вернётся true)
// или не истечёт заданное время (при этом вернётся false). Если time=0, таймаут не ограничен.
// Пример 1: os_waitEvent(event1, 1000);
// Пример 2: if ( os_waitEvent(event1, 1000) ) {} else {};
_ALWAYS_INLINE bool os_waitEvent(Event *event, uint32_t time)
{
    *event = NOTRIGGERED;
    taskArray[currentTask].lockObject = (uint8_t *)event;
    if (time)
    {
        taskArray[currentTask].state = WAIT_TIMEOUT;
        taskArray[currentTask].delay = time;
    }
    else
    {
        taskArray[currentTask].state = WAITING;
    }
    os_yield();
    if (*taskArray[currentTask].lockObject == TRIGGERED) // FIXME
    {
        return true ;
    }
    else
    {
        return false;
    }
}

// Сигнализирование о том, что событие произошло. Пример: os_eventOccurred(&event1);
_ALWAYS_INLINE void os_occurredEvent(Event *event)
{
    *event = TRIGGERED;          // событие произошло
}

// Ожидание и захват мьютекса
_ALWAYS_INLINE void os_lockMutex(Mutex *mutex)
{
    taskArray[currentTask].lockObject = (uint8_t *)mutex;
    taskArray[currentTask].state = WAITING;
    os_yield();
    *taskArray[currentTask].lockObject = LOCK; // к mutex уже обратиться нельзя
}

// Освобождение мьютекса
_ALWAYS_INLINE void os_unlockMutex(Mutex *mutex)
{
    *mutex = UNLOCK;
}

// МАКРОС! Приостановка потока до тех пор, пока не выполнится условие. Пример: OS_WAIT(a>5 && b==3);
#define OS_WAIT(expression) do {                 \
    _os_setjmp(taskArray[currentTask].context); \
    (expression) ? 0 : longjmp(_os_context, 1);   \
} while(0)

#endif // LOWOS_H
