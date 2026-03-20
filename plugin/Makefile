TARGET = PspAchievements
OBJS = src/main.o src/profile.o src/game_db.o src/game_map.o src/detect.o src/popup.o src/memory.o src/rcheevos_glue.o

BUILD_PRX = 1
PRX_EXPORTS = exports.exp

USE_KERNEL_LIBC = 1
USE_KERNEL_LIBS = 1

INCDIR = src
CFLAGS = -O2 -G0 -Wall -Wextra -std=c99
LDFLAGS =
LIBS = -lpspdisplay -lpspctrl

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak