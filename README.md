# Device Mapper Proxy (dmp) Kernel Module

В задании предлагается реализовать модуль ядра под ОС Linux, который создает виртуальные блочные устрйоства поверх существующего на базе device mapper и следит за статистикой выполняемых операций на устройстве.

## 📌 Описание

Модуль ядра Linux "dmp" реализует простой таргет для Device Mapper. Он создаёт виртуальное блочное устройство, перенаправляющее I/O-запросы на базовое устройство и собирает статистику чтения/записи в реальном времени через "sysfs".


Модуль протестирован на:

- Debian 12.11

---

# Клонирование репозитория

```bash
git clone https://github.com/keyfda/test-task-module.git
cd test-task-module
```

## ⚙️ Установка зависимостей

Перед сборкой необходимо установить заголовки ядра и инструменты сборки.

### Debian

```bash
sudo apt update
sudo apt install build-essential linux-headers-$(uname -r)
```


Запуск сборки
```bash
make
```
Убедитесь, что появился .ko файл модуля.

## Загрузка модуля


```bash
sudo insmod dmp.ko
```

Проверьте, что модуль загружен:
```bash
lsmod | grep dmp
```

А также dmesg
```bash
sudo dmesg | tail
```


### Устновка и тестирование устройства

Создание блочного устройства
```bash
sudo dmsetup create zero1 --table "0 $size zero"
```
Note: $size - произвольный размер устройства.

Проверьте, что устройство успешно создалось
```bash
ls -al /dev/mapper/
```


Далее необходимо создать proxy-устрйоство

```bash
sudo dmsetup create dmp1 --table "0 $size dmp /dev/mapper/zero1"
```

Note: $size - размер устрйоства /dev mapper/zero1.

# Тестирование

Ввод
```bash
sudo dd if=/dev/random of=/dev/mapper/dmp1 bs=4k count=1
```
Вывод
```bash
sudo dd of=/dev/null if=/dev/mapper/dmp1 bs=4k count=1
```


## Проверка статистики
Статистика открывается через 
```bash
cat /sys/kernel/dmp
```
В этой директории находятся 6 файлов:
avg_size - среднйи размер блока всех операций
avg_write_size - средний размер блока записи
avg_read_size - средний размер блока чтения
write_reqs - количество операций записи
read-reqs - количесвто операций чтения
total_reqs - общее количество операций


