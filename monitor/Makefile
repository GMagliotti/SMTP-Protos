#flags
CFLAGS:= -std=c11 -pedantic -pedantic-errors -g -Wall  -Wextra -D_POSIX_C_SOURCE=200112L -Werror -fsanitize=address 
SMTPD_CLI:= monitor.elf
LIB_OBJS:= build/selector.o build/monitor.o
MAIN_OBJ:= build/main.o

all: $(SMTPD_CLI)

$(SMTPD_CLI): $(LIB_OBJS) $(MAIN_OBJ)
	$(CC) $(CFLAGS) $(LIB_OBJS) $(MAIN_OBJ) -o $(SMTPD_CLI)

clean:
	- rm -rf $(SMTPD_CLI) build/*.o

build/%.o: %.c
	mkdir -p build
	$(CC) -c $(CFLAGS) $< -o $@

build/main.o: main.c
	mkdir -p build
	$(CC) -c $(CFLAGS) $< -o $@