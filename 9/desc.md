# Отчёт 1.

## Условие
```
20. Задача об умных цветах. На клумбе растет 10 цветов, за ними непрерывно следят два садовника и поливают увядшие цветы, при этом оба садовника очень боятся полить один и тот же цветок, который еще не начал вянуть. Иначе он пропадет из–за перелива.
Создать многопроцессное приложение, моделирующее состояния цветов на клумбе и действия садовников.
Каждый цветок — отдельный процесс который следит за своим состоянием и информирует, что он полит, увядает (нужно полить), засох или перелит и сгнил (бесполезно поливать). Каждый садовник задается отдельным потоком.
```

## Сценарий взаимодействия и детали реализации

```
Потоки садовники следят за очередью, в которую процессы цветы добавляют позиции. Процессы цветы (изначально состояние WATERED) по истечении времени time_without_water проверяют полили ли их садовники (исполнена ли позиция в очереди), если да, то состояние цветка становится NEED_WATER. 

Соответственно, после добавления в очередь цветы поливаются садовником (состояние цветка переходит в WATERED) за время равное time_to_water (поток ждёт). Также садовник может ошибиться (вероятность 20%) и налить воды слишком много (цветок перейдёт в состояние OVERWATERED), тогда цветок умрёт (DEAD).

Очередь представляет собой кольцевой буффер и синхронизирована мьютексом (реализован в виде бинарного семафора) для операций (queue_mutex) и счётными семафорами для доступа к очереди (queue_empty, queue_full). Также для каждого цветка есть семафор, чтобы не допустить гонку данных.

Программа может завершиться, когда все цветы умрут или подачей сигнала SIGINT(при этом все процессы убиваются, семафоры уничтожаются, общая память освобождается)
```

## Отображение сущностей на процессы и потоки
| Сущность                  | В программе                                  |
| ---------------------------------- | -------------------------------------------- |
| Цветок                             | 10 процессов              |
| Клумба (общая информация о цветах) | Разделяемая память POSIX                 |
| Состояние каждого цветка           | Структура `struct Flower` в shm с enum state            |
| Садовник                           | Отдельный поток внутри главного процесса |
| Очередь цветков                    | Очередь в разделяемой памяти                 |
| Синхронизация                      | Набор именованных POSIX семафоров        |

## Процессы-цветки
Каждый процесс-цветок работает независимо:

1. Периодически меняет состояние:
  WATERED -> NEED_WATER -> DEAD или OVERWATERED

2. При переходе в состояние NEED_WATER:

  - ставит себя в очередь на полив;

  - уведомляет садовников;

3. Если долго не поливать — умирает.

## Садовники (потоки)

Главный процесс создаёт 2 потока:

- оба извлекают id цветка из очереди;

- блокируют семафор цветка (flower_sem[id]);

- поливают его (смена состояния на WATERED или OVERWATERED с вероятностью 20%);

- разблокируют и продолжают ждать следующего задания.

Благодаря flower_sem[id] два садовника никогда не поливают один цветок одновременно.

## Процесс наблюдатель

Процесс создаёт неблокирующую очередь сообщений максимального размера на моей системе (если выдаёт ошибку invalid argument, нужно уменьшить параметр QUEUE_SIZE):

- главный и процессы цветов посылают в очередь сообщения (логи)

- все сообщения ожидаются очередью в цикле

- ресурсы очереди очищаются либо после уничтожения процесса наблюдателя, либо главного процесса, в зависимости от того, что раньше закончится 

## Разделяемая память
в shm хранятся:
```
struct Flower flowers[NUM_PROCS];
queue_t queue;
int system_running;
int alive_flowers;
```

## Именованные семафоры
| Имя             | Назначение                    |
| --------------- | ----------------------------- |
| `/queue_mutex`  | защита структуры очереди      |
| `/queue_empty`  | очередь пуста                 |
| `/queue_full`   | очередь содержит элементы     |
| `/flower_sem_X` | семафор конкретного цветка    |
| `/alive_sem`    | корректный учёт смерти цветов |
Все семафоры корректно удаляются после завершения программы.

## Параметры

- SHM_NAME - название разделяемой памяти
- QUEUE_SIZE - размер очереди
- NUM_PROCS - количество процессов цветков
- NUM_THRS - количество потоков
- TIME_TO_WATER - количество времени, нужное чтобы полить цветок
- TIME_WITHOUT_WATER - количество времени, которое может жить цветок без воды
- LOG_QUEUE - название очереди
- LOG_MSG_SIZE - размер сообщения, поступающего в очередь

## Компиляция и запуск

### Компиляция
Из папки, в которой содержится программа:
```bash
gcc observe_proc.c -o observe_proc -lrt
gcc ./main_proc.c -o main_proc -lpthread -lrt -g
gcc ./flower_proc.c -o flower_proc -lrt
```

### Запуск
Запускать надо именно в такой последовательности.\
1) Открыть одну консоль и запустить процесс наблюдатель:
```
./observe_proc
```
2) Открыть одну консоль и запустить главный процесс:
```
./main_proc
```
Главный процесс будет ждать, пока 10 процессов цветов не запустятся.\
3. Открыть 10 отдельных консолей и запустить цветы:
```
./flower_proc <id>
```
id - от 0 до 9 \
Или лучше всего сделать так:
```
./observe_proc
chmod +x run.sh
./run.sh
```

## Завершение системы
Можно либо убивать цветки (Ctrl+C) отдельно в консолях, тогда умрёт главный процесс. Или можно убить главный процесс (Ctrl+C), тогда процессы цветки и наблюдатель умрут. Можно просто запустить скрипт и дождаться, когда все цветы умрут. Семафоры и разделяемая память очищается, потоки завершаются, процесс наблюдатель также убивается.

## Соответствование требованиям

- используется очередь сообщений POSIX
- используются именованные семафоры POSIX
- используется разделяемая память POSIX
- процессы завершаются по сигналу
- выводятся логи

## Пример работы:
Главная консоль:
```
Ожидаю запуска всех 10 процессов-цветов...
Запущено 0/10 процессов. Ожидаю...
Запущено 0/10 процессов. Ожидаю...
Запущено 2/10 процессов. Ожидаю...
Запущено 5/10 процессов. Ожидаю...
Запущено 7/10 процессов. Ожидаю...
Все 10 процессов запущены!
[Gardener 0] started
[gardener 0] watering flower 0
[MAIN] System started. Press Ctrl+C to stop or wait, when all flowers will die.
[MAIN] this much flowers are alive now: 10
[Gardener 1] started
[MAIN] this much flowers are alive now: 10
[gardener 0] finished watering flower 0
[gardener 1] watering flower 1
[MAIN] this much flowers are alive now: 9
[MAIN] this much flowers are alive now: 9
[gardener 1] finished watering flower 1
[gardener 0] watering flower 5
[MAIN] this much flowers are alive now: 9
[MAIN] this much flowers are alive now: 9
[gardener 0] finished watering flower 5
[gardener 0] watering flower 1
[MAIN] this much flowers are alive now: 8
[MAIN] this much flowers are alive now: 8
[gardener 0] finished watering flower 1
[gardener 0] watering flower 2
[MAIN] this much flowers are alive now: 8
[MAIN] this much flowers are alive now: 8
[gardener 0] finished watering flower 2
[gardener 0] watering flower 1
[MAIN] this much flowers are alive now: 8
[MAIN] this much flowers are alive now: 8
[gardener 0] finished watering flower 1
[gardener 0] watering flower 4
[MAIN] this much flowers are alive now: 7
[MAIN] this much flowers are alive now: 6
[gardener 0] finished watering flower 4
[MAIN] this much flowers are alive now: 6
[gardener 1] watering flower 7
[MAIN] this much flowers are alive now: 6
[gardener 1] finished watering flower 7
[gardener 1] watering flower 4
[MAIN] this much flowers are alive now: 6
[MAIN] this much flowers are alive now: 5
[gardener 1] finished watering flower 4
[MAIN] this much flowers are alive now: 5
[gardener 0] watering flower 2
[MAIN] this much flowers are alive now: 5
[gardener 0] finished watering flower 2
[gardener 1] watering flower 4
[MAIN] this much flowers are alive now: 5
[MAIN] this much flowers are alive now: 4
[gardener 1] finished watering flower 4
[gardener 1] watering flower 7
[MAIN] this much flowers are alive now: 4
[MAIN] this much flowers are alive now: 4
[MAIN] this much flowers are alive now: 3
[gardener 1] finished watering flower 7
[gardener 0] watering flower 4
[MAIN] this much flowers are alive now: 3
[gardener 0] finished watering flower 4
[MAIN] this much flowers are alive now: 3
[gardener 0] watering flower 7
[MAIN] this much flowers are alive now: 2
[gardener 0] finished watering flower 7
[gardener 1] watering flower 4
[MAIN] this much flowers are alive now: 2
[MAIN] this much flowers are alive now: 2
[gardener 1] finished watering flower 4
[gardener 1] watering flower 7
[MAIN] this much flowers are alive now: 2
[MAIN] this much flowers are alive now: 2
[gardener 1] finished watering flower 7
[MAIN] this much flowers are alive now: 2
[gardener 0] watering flower 4
[MAIN] this much flowers are alive now: 2
[gardener 0] finished watering flower 4
[MAIN] this much flowers are alive now: 2
[gardener 1] watering flower 7
[MAIN] this much flowers are alive now: 2
[MAIN] this much flowers are alive now: 2
[gardener 1] finished watering flower 7
[gardener 0] watering flower 4
[MAIN] this much flowers are alive now: 2
[gardener 0] finished watering flower 4
[MAIN] this much flowers are alive now: 2
[gardener 0] watering flower 7
[MAIN] this much flowers are alive now: 2
[gardener 0] finished watering flower 7
[gardener 1] watering flower 4
[MAIN] this much flowers are alive now: 2
[MAIN] this much flowers are alive now: 2
[gardener 1] finished watering flower 4
[MAIN] this much flowers are alive now: 2
[gardener 1] watering flower 7
[MAIN] this much flowers are alive now: 2
[gardener 1] finished watering flower 7
[gardener 0] watering flower 4
[MAIN] this much flowers are alive now: 2
[MAIN] this much flowers are alive now: 2
[gardener 0] finished watering flower 4
[gardener 0] watering flower 7
[MAIN] this much flowers are alive now: 2
[MAIN] this much flowers are alive now: 2
[gardener 0] finished watering flower 7
[gardener 1] watering flower 4
[MAIN] this much flowers are alive now: 2
[MAIN] this much flowers are alive now: 2
[gardener 1] finished watering flower 4
[gardener 0] watering flower 7
[MAIN] this much flowers are alive now: 2
[MAIN] this much flowers are alive now: 2
[gardener 0] finished watering flower 7
[gardener 1] watering flower 4
[MAIN] this much flowers are alive now: 2
[MAIN] this much flowers are alive now: 2
[gardener 1] finished watering flower 4
[MAIN] this much flowers are alive now: 2
[gardener 0] watering flower 7
[MAIN] this much flowers are alive now: 2
[gardener 0] finished watering flower 7
[gardener 1] watering flower 4
[MAIN] this much flowers are alive now: 2
[MAIN] this much flowers are alive now: 2
[gardener 1] finished watering flower 4
[MAIN] this much flowers are alive now: 2
[gardener 0] watering flower 7
[MAIN] this much flowers are alive now: 1
[gardener 0] finished watering flower 7
[MAIN] this much flowers are alive now: 1
[MAIN] this much flowers are alive now: 0
[Gardener 1] exiting
[Gardener 0] exiting
[MAIN] Cleanup started
[MAIN] Cleanup completed
```
Консоль цветка (пример):
```
[Flower #9] Started. PID=27021
[Flower #9] needs water, pushing to queue
[Flower #9] died: DECAYED
```
Консоль наблюдателя:
```
[OBSERVER] Starting observer...
[OBSERVER] Created new queue
[LOG] Ожидаю запуска всех 10 процессов-цветов...
[LOG] Запущено 0/10 процессов. Ожидаю...
[LOG] Запущено 0/10 процессов. Ожидаю...
[LOG] [Flower #0] Started. PID=48692
[LOG] [Flower #1] Started. PID=48703
[LOG] Запущено 2/10 процессов. Ожидаю...
[LOG] [Flower #2] Started. PID=48719
[LOG] [Flower #3] Started. PID=48729
[LOG] Запущено 4/10 процессов. Ожидаю...
[LOG] [Flower #4] Started. PID=48739
[LOG] [Flower #5] Started. PID=48749
[LOG] [Flower #6] Started. PID=48759
[LOG] Запущено 7/10 процессов. Ожидаю...
[LOG] [Flower #0] needs water, pushing to queue
[LOG] [Flower #7] Started. PID=48769
[LOG] [Flower #3] needs water, pushing to queue
[LOG] [Flower #1] needs water, pushing to queue
[LOG] [Flower #8] Started. PID=48780
[LOG] Запущено 9/10 процессов. Ожидаю...
[LOG] [Flower #9] Started. PID=48790
[LOG] [Flower #2] needs water, pushing to queue
[LOG] [Flower #5] needs water, pushing to queue
[LOG] [Flower #6] needs water, pushing to queue
[LOG] Все 10 процессов запущены!
[LOG] [Gardener 0] started
[LOG] [MAIN] System started. Press Ctrl+C to stop or wait, when all flowers will die.
[LOG] [MAIN] this much flowers are alive now: 10
[LOG] [gardener 0] watering flower 0
[LOG] [Gardener 1] started
[LOG] [Flower #4] needs water, pushing to queue
[LOG] [Flower #3] died: DECAYED
[LOG] [Flower #8] needs water, pushing to queue
[LOG] [MAIN] this much flowers are alive now: 9
[LOG] [Flower #9] needs water, pushing to queue
[LOG] [Flower #7] needs water, pushing to queue
[LOG] [Flower #5] died: DECAYED
[LOG] [Flower #1] died: DECAYED
[LOG] [Flower #6] died: DECAYED
[LOG] [gardener 0] finished watering flower 0
[LOG] [gardener 0] watering flower 2
[LOG] [Flower #0] needs water, pushing to queue
[LOG] [MAIN] this much flowers are alive now: 6
[LOG] [MAIN] this much flowers are alive now: 6
[LOG] [gardener 0] finished watering flower 2
[LOG] [Flower #2] needs water, pushing to queue
[LOG] [MAIN] this much flowers are alive now: 6
[LOG] [gardener 1] watering flower 9
[LOG] [MAIN] this much flowers are alive now: 6
[LOG] [gardener 1] finished watering flower 9
[LOG] [gardener 1] watering flower 2
[LOG] [Flower #9] needs water, pushing to queue
[LOG] [MAIN] this much flowers are alive now: 6
[LOG] [Flower #7] died: DECAYED
[LOG] [MAIN] this much flowers are alive now: 5
[LOG] [gardener 1] finished watering flower 2
[LOG] [Flower #2] needs water, pushing to queue
[LOG] [MAIN] this much flowers are alive now: 5
[LOG] [gardener 1] watering flower 8
[LOG] [gardener 1] finished watering flower 8
[LOG] [MAIN] this much flowers are alive now: 5
[LOG] [gardener 1] watering flower 2
[LOG] [Flower #8] needs water, pushing to queue
[LOG] [Flower #9] died: DECAYED
[LOG] [MAIN] this much flowers are alive now: 5
[LOG] [Flower #4] died: DECAYED
[LOG] [Flower #0] died: DECAYED
[LOG] [gardener 1] finished watering flower 2
[LOG] [MAIN] this much flowers are alive now: 2
[LOG] [gardener 0] watering flower 8
[LOG] [Flower #2] died: OVERWATERED
[LOG] [gardener 0] finished watering flower 8
[LOG] [MAIN] this much flowers are alive now: 1
[LOG] [Flower #8] needs water, pushing to queue
[LOG] [gardener 1] watering flower 8
[LOG] [MAIN] this much flowers are alive now: 1
[LOG] [gardener 1] finished watering flower 8
[LOG] [MAIN] this much flowers are alive now: 1
[LOG] [Flower #8] needs water, pushing to queue
[LOG] [MAIN] this much flowers are alive now: 1
[LOG] [gardener 0] watering flower 8
[LOG] [MAIN] this much flowers are alive now: 1
[LOG] [gardener 0] finished watering flower 8
[LOG] [Flower #8] needs water, pushing to queue
[LOG] [gardener 1] watering flower 8
[LOG] [MAIN] this much flowers are alive now: 1
[LOG] [gardener 1] finished watering flower 8
[LOG] [MAIN] this much flowers are alive now: 1
[OBSERVER] Shutdown requested
[OBSERVER] Cleanup started
[OBSERVER] exiting.
```