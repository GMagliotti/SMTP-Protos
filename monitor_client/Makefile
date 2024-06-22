CFLAGS:= -std=c11 -pedantic -pedantic-errors -g -Wall  -Wextra -D_POSIX_C_SOURCE=200112L -Werror -fsanitize=address 
SMTPD_CLI:= client_monitor.elf
MAIN_OBJ:= build/client_monitor.o

all: $(SMTPD_CLI)

$(SMTPD_CLI): $(MAIN_OBJ)
	$(CC) $(CFLAGS) $(MAIN_OBJ) -o $(SMTPD_CLI)

clean:
	- rm -rf $(SMTPD_CLI) build/*.o

build/%.o: %.c
	mkdir -p build
	$(CC) -c $(CFLAGS) $< -o $@

build/client_monitor.o: client_monitor.c
	mkdir -p build
	$(CC) -c $(CFLAGS) $< -o $@