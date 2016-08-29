#include "lowos.h"
#ifdef __linux
#include "port_linux.h"
#elif __arm__
#include "port_arm.h"
#endif

_task taskArray[MAX_TASKS] = {};         // очередь задач
static volatile uint8_t arrayTail = 0;  // "хвост" очереди
volatile uint8_t currentTask = 0;       // текущая задача
jmp_buf _os_context;                     // контекст планировщика

// =================================================================================================

void os_initOS(void)                      // инициализация системы
{
    currentTask = 0;
    arrayTail = 0;
    sysTimerInit();
}

void SysTick_Handler(void)              // обработчик прерывания системного таймера
{
    for (uint8_t i = 0; i < arrayTail; i++)
    {
        if (taskArray[i].delay)
        {
            taskArray[i].delay--;
        }
    }
}

void os_initTask(void (*taskFunc)(void)) // функция инициализации задачи
{
    if (!taskFunc)
    {
        OS_ERROR();
        return;
    }

    if (arrayTail < MAX_TASKS)          // если такой задачи в списке нет и есть место,то добавляем
    {
        // DISABLE_INTERRUPT;
        taskArray[arrayTail].state = NOINIT;
        taskArray[arrayTail].delay = 0;
        taskArray[arrayTail].pFunc = taskFunc;
        arrayTail++;                    // увеличиваем "хвост"
        //RESTORE_INTERRUPT;
    }
}

_ALWAYS_INLINE void _os_return_in_task()
{
    taskArray[currentTask].state = ACTIVE;
    longjmp(taskArray[currentTask].context, 1); // возвращаемся в задачу
}

void os_loop(void)
{
    for (currentTask = 0 ; ; ++currentTask == arrayTail ? currentTask = 0 : 0) // Цикл по кругу
    {
        if (setjmp(_os_context)) // Сохранить метку, куда вернуться
        {
            continue;           // Если вернулись из задачи - начинаем работу со следующией задачей
        }

        switch (taskArray[currentTask].state)               // оцениваем состояние задачи
        {
            case NOINIT:                                    // задача ещё ни разу не запускалась
                taskArray[currentTask].state = ACTIVE;
                taskArray[currentTask].pFunc();             // запустим её
                OS_ERROR(); // WTF? задача отдала управление диспетчеру, сюда мы не должны попасть
                taskArray[currentTask].state = NOINIT;      // в следующий проход её перезапустим
                break;

            case DELAYED:                                   // задача на паузе
                if (taskArray[currentTask].delay == 0)      // по окончании которой
                {
                    taskArray[currentTask].state = ACTIVE;
                    longjmp(taskArray[currentTask].context, 1); // возвращаемся в задачу
                }
                break;

            case WAITING:                                   // чего-то ждём (mutex, event)
                if (* taskArray[currentTask].lockObject == 0)    // оцениваем состояние события
                {
                    taskArray[currentTask].state = ACTIVE;
                    longjmp(taskArray[currentTask].context, 1); // возвращаемся в задачу
                }
                break;

            case WAIT_TIMEOUT:                              // ждём события или таймаута
                if ((* taskArray[currentTask].lockObject == 0) || (taskArray[currentTask].delay == 0))
                {
                    taskArray[currentTask].state = ACTIVE;
                    longjmp(taskArray[currentTask].context, 1); // возвращаемся в задачу
                }
                break;

            case ACTIVE:                                    // задача запущена
                longjmp(taskArray[currentTask].context, 1); // просто возвращаемся в задачу
                break;

            default:
                OS_ERROR();
        }
    }
}
