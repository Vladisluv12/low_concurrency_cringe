# Отчёт 1. Чепурных Владислав Евгеньевич, БПИ-247

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

Очередь представляет собой кольцевой буффер и синхронизирована мьютексом (реализован в виде бинарного семафора) для операций (mu) и счётными семафорами для доступа к очереди (empty, ful). Также для каждого цветка есть семафор, чтобы не допустить гонку данных.

Программа может завершиться, когда все цветы умрут или подачей сигнала SIGINT(при этом все процессы убиваются, семафоры уничтожаются, общая память освобождается)
```

## Структуры данных

  1. Flower - цветок с состоянием и временем полива и временем без воды

  2. queue_t - кольцевая очередь для хранения id цветов, нуждающихся в поливе

  3. shared_mem_t - разделяемая память содержит массив цветов и очередь

## Синхронизация

1. Семафоры для цветков:

    По одному на каждый цветок

    Гарантируют атомарный доступ к данным цветка

    Предотвращают race condition при изменении состояния

2. Семафоры для очереди:

    mu - бинарный семафор для эксклюзивного доступа к очереди

    empty - счетчик свободных мест (инициализирован в 20)
    
    full - счетчик занятых мест (инициализирован в 0)


## Компиляция и запуск
Из папки, в которой содержится программа:
```bash
gcc fst_console_program.c -o flowers -g -lpthread
./flowers
```

## Соответствие требованиям

1. Множество процессов взаимодействуют с использованием неименованных POSIX семафоров. - есть
2. Обмен данными ведется через разделяемую память в стандарте POSIX. - есть
3. Реализовать завершение программы в соответствии с условием задачи, а также предусмотреть корректное завершение всей программы по прерыванию с клавиатуры при подаче соответствующего сигнала. - есть
4. Каждый из процессов выводит в общий поток данных информацию необходимую для понимания его работы в соответствии с выполняемой им ролевой функцией. - есть
5. В программе предусмотреть удаление семафоров и разделяемой памяти по ее завершению любым из способов. - есть

## Пример работы:

```
flower process 0 (pid 24447) started
flower process 1 (pid 24448) started
flower process 2 (pid 24449) started
flower process 3 (pid 24450) started
flower process 4 (pid 24451) started
flower process 5 (pid 24452) started
flower process 6 (pid 24453) started
flower process 7 (pid 24454) started
flower process 8 (pid 24455) started
flower process 9 (pid 24456) started
gardener 1 started
gardener 2 started
flower 7 needs water
flower 7 added to queue
gardener 1 watering flower 7
flower 8 needs water
flower 8 added to queue
gardener 2 watering flower 8
flower 3 needs water
flower 3 added to queue
flower 9 needs water
flower 9 added to queue
flower 2 needs water
flower 2 added to queue
flower 4 needs water
flower 4 added to queue
gardener 2 finished watering flower 8
gardener 2 watering flower 3
flower 0 needs water
flower 0 added to queue
flower 6 needs water
flower 6 added to queue
flower 1 needs water
flower 1 added to queue
flower 5 needs water
flower 5 added to queue
gardener 1 finished watering flower 7
gardener 1 watering flower 9
flower 7 needs water
flower 7 added to queue
flower 8 needs water
flower 8 added to queue
gardener 1 finished watering flower 9
gardener 1 watering flower 2
flower 9 needs water
flower 9 added to queue
gardener 2 finished watering flower 3
gardener 2 watering flower 4
flower 3 needs water
flower 3 added to queue
flower 7 died
flower process 7 exiting
flower 0 died
flower process 0 exiting
flower 6 died
flower process 6 exiting
gardener 1 finished watering flower 2
gardener 1 watering flower 1
flower 2 needs water
flower 2 added to queue
flower 8 died
flower process 8 exiting
flower 5 died
flower process 5 exiting
flower 9 died
flower process 9 exiting
flower 3 died
flower process 3 exiting
gardener 2 finished watering flower 4
gardener 2 watering flower 2
flower 4 died
flower process 4 exiting
gardener 1 finished watering flower 1
flower 1 needs water
flower 1 added to queue
gardener 1 watering flower 1
gardener 2 finished watering flower 2
flower 2 needs water
flower 2 added to queue
gardener 2 watering flower 2
gardener 1 finished watering flower 1
gardener 2 finished watering flower 2
flower 1 needs water
flower 1 added to queue
gardener 1 watering flower 1
flower 2 died
flower process 2 exiting
gardener 1 finished watering flower 1
flower 1 needs water
flower 1 added to queue
gardener 2 watering flower 1
gardener 2 finished watering flower 1
flower 1 needs water
flower 1 added to queue
gardener 1 watering flower 1
gardener 1 finished watering flower 1
flower 1 needs water
flower 1 added to queue
gardener 2 watering flower 1
gardener 2 finished watering flower 1
flower 1 needs water
flower 1 added to queue
gardener 1 watering flower 1
gardener 1 finished watering flower 1
flower 1 needs water
flower 1 added to queue
gardener 2 watering flower 1
gardener 2 finished watering flower 1
flower 1 needs water
flower 1 added to queue
gardener 1 watering flower 1
gardener 1 finished watering flower 1
flower 1 needs water
flower 1 added to queue
gardener 2 watering flower 1
gardener 2 finished watering flower 1
flower 1 needs water
flower 1 added to queue
gardener 1 watering flower 1
gardener 1 finished watering flower 1
flower 1 needs water
flower 1 added to queue
gardener 2 watering flower 1
gardener 2 finished watering flower 1
flower 1 needs water
flower 1 added to queue
gardener 1 watering flower 1
gardener 1 finished watering flower 1
flower 1 needs water
flower 1 added to queue
gardener 2 watering flower 1
gardener 2 finished watering flower 1
flower 1 needs water
flower 1 added to queue
gardener 1 watering flower 1
gardener 1 finished watering flower 1
flower 1 needs water
flower 1 added to queue
gardener 2 watering flower 1
gardener 2 finished watering flower 1
flower 1 needs water
flower 1 added to queue
gardener 1 watering flower 1
gardener 1 finished watering flower 1
flower 1 needs water
flower 1 added to queue
gardener 2 watering flower 1
gardener 2 finished watering flower 1
flower 1 needs water
flower 1 added to queue
gardener 1 watering flower 1
gardener 1 finished watering flower 1
flower 1 needs water
flower 1 added to queue
gardener 2 watering flower 1
gardener 2 finished watering flower 1
flower 1 needs water
flower 1 added to queue
gardener 1 watering flower 1
gardener 1 finished watering flower 1
flower 1 died
flower process 1 exiting
gardener 2 exiting
gardener 1 exiting
Main process finished
```