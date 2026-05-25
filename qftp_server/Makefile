SOURCES = src/main.c src/session.c src/sysutil.c src/ftpproto.c src/privparent.c src/str.c src/tunable.c \
src/parseconf.c src/privsock.c src/hash.c

CFLAGS += -Wall -Werror -O1 #-s 
LDFLAGS += -lcrypt

ifeq ($(CC),cc)
CC=${CROSS_COMPILE}gcc
endif

linux: clean
	$(shell if [ -e  out]; then echo "file existed";else mkdir out; fi;)
	${CC} $(CFLAGS) $(SOURCES) -o ./out/QFtpServer ${LDFLAGS} -I src/ -I ../ 

clean:
	rm -rf out/*
