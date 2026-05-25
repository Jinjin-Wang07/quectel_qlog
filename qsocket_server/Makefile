SOURCES = src/main.c src/qparameter.c src/common.c src/qfile.c src/qsocket.c

CFLAGS += -Wall  -Wno-unused-function -Werror -O1 #-s 

LDFLAGS += -lpthread

OUT_DIR := bin

ifeq ($(CC),cc)
CC=${CROSS_COMPILE}gcc
endif

linux: check_dir clean
	${CC} $(CFLAGS) $(SOURCES) -o $(OUT_DIR)/QSocketServer ${LDFLAGS} -I src/ 

clean:
	rm -rf $(OUT_DIR)/QSocketServer

# 检查目录是否存在，如果不存在就创建它
check_dir:
	@if [ ! -d "$(OUT_DIR)" ]; then \
		echo "Directory '$(OUT_DIR)' does not exist. Creating..."; \
		mkdir -p "$(OUT_DIR)"; \
	else \
		echo "Directory '$(OUT_DIR)' already exists."; \
	fi
