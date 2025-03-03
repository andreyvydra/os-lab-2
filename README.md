## Вводное
Вариант: clock\
ОС: windows

## Задание
Для оптимизации работы с блочными устройствами в ОС существует кэш страниц с данными, которыми мы производим операции чтения и записи на диск. Такой кэш позволяет избежать высоких задержек при повторном доступе к данным, так как операция будет выполнена с данными в RAM, а не на диске (вспомним пирамиду памяти).

В данной лабораторной работе необходимо реализовать блочный кэш в пространстве пользователя в виде динамической библиотеки (dll или so). Политику вытеснения страниц и другие элементы задания необходимо получить у преподавателя.

При выполнении работы необходимо реализовать простой API для работы с файлами, предоставляющий пользователю следующие возможности:

1. Открытие файла по заданному пути файла, доступного для чтения. Процедура возвращает некоторый хэндл на файл. Пример:\
```c++
int lab2_open(const char *path).
```

2. Закрытие файла по хэндлу. Пример:
```c++
int lab2_close(int fd).
```

3. Чтение данных из файла. Пример:
```c++
ssize_t lab2_read(int fd, void buf[.count], size_t count).
```

4. Запись данных в файл. Пример:
```c++
ssize_t lab2_write(int fd, const void buf[.count], size_t count).
```

5. Перестановка позиции указателя на данные файла. Достаточно поддержать только абсолютные координаты. Пример:
```c++
off_t lab2_lseek(int fd, off_t offset, int whence).
```

6. Синхронизация данных из кэша с диском. Пример:
```c++
int lab2_fsync(int fd).
```

Операции с диском разработанного блочного кеша должны производиться в обход page cache используемой ОС.

## Реализация
Основные структуры и объекты 

```c++
struct CacheBlock {
    char* data;
    bool is_dirty; // Изменен ли блок
    bool was_accessed; // Был ли блок недавно использован
};

struct FileDescriptor {
    HANDLE file_handle;
    LONGLONG file_id; // Уникальный идентификатор файла
    LARGE_INTEGER offset;
};

typedef std::pair<LONGLONG, LONGLONG> CacheKey; // (file_id, block_id)

std::map<HANDLE, FileDescriptor> fd_table;
std::map<CacheKey, CacheBlock> cache_table;
std::vector<CacheKey> cache_order;
```

## Результаты

Чтение с диска без использования кэша (10 итераций)

```bash
Overall Stats:
Average read latency: 0.0104094 seconds
Minimum read latency: 0.0085179 seconds
Maximum read latency: 0.0136093 seconds
```

Чтение с диска с использованием кэша Clock (10 итераций)

```bash
Overall Stats:
Average read latency: 0.00165789 seconds
Minimum read latency: 0.0003468 seconds
Maximum read latency: 0.0128674 seconds

Cache hits: 2349
Cache misses: 266
```

Запись на диск без использования кэша (10 итераций)

```bash
Overall Stats:
Average write latency: 0.0291669 seconds
Minimum write latency: 0.0113648 seconds
Maximum write latency: 0.0398694 seconds
```

Запись на диск с использованием кэша (10 итераций)

```bash
Overall Stats:
Average write latency: 0.00457321 seconds
Minimum write latency: 0.0006564 seconds
Maximum write latency: 0.0374397 seconds

Cache hits: 2304
Cache misses: 256
```

## Заключение

При проведении замеров времени работы программ было подтверждено, что работа совместная с кэшом при примерном попадании при кэш рейте 80-90% примерно в 5-10 раз быстрее.
