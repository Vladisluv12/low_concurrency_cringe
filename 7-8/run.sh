#!/bin/bash

FLOWER="./flower_proc"
MAIN="./main_proc"

# Проверяем что файлы существуют
if [ ! -f "$FLOWER" ]; then
    echo "Ошибка: не найден flower_proc"
    exit 1
fi

if [ ! -f "$MAIN" ]; then
    echo "Ошибка: не найден main_proc"
    exit 1
fi

# Определяем доступный терминал
open_terminal() {
    if command -v gnome-terminal >/dev/null 2>&1; then
        gnome-terminal -- bash -c "$1; exec bash"
    elif command -v konsole >/dev/null 2>&1; then
        konsole -e bash -c "$1; exec bash"
    elif command -v xfce4-terminal >/dev/null 2>&1; then
        xfce4-terminal -- bash -c "$1; exec bash"
    elif command -v xterm >/dev/null 2>&1; then
        xterm -hold -e "$1"
    elif command -v mate-terminal >/dev/null 2>&1; then
        mate-terminal -- bash -c "$1; exec bash"
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        # macOS Terminal
        osascript -e "tell app \"Terminal\"
            do script \"$1\"
        end tell"
    else
        echo "Не найден поддерживаемый терминал!"
        exit 1
    fi
}

echo "Запускаю main_proc..."
open_terminal "$MAIN"

sleep 1

echo "Запускаю 10 процессов-цветов..."
for i in $(seq 0 9); do
    open_terminal "$FLOWER $i"
    sleep 0.2
done

echo "Готово!"
