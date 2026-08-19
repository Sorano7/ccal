cc \
    src/main.c \
    src/digit.c \
    src/lexer.c \
    src/vm.c \
    -O2 \
    -Wall -Wextra \
    -Iinclude -lgmp \
    "$@" \
    -o build/ccal
