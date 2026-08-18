cc src/*.c \
    -Wall -Wextra \
    -Iinclude -lgmp \
    "$@" \
    -o build/ccal
