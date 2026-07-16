TARGET = PSPAchievementsNG

OBJS = src/main.o src/config.o src/logger.o src/game_detector.o src/game_context.o src/memory.o src/memory_watch.o src/package.o src/badge.o src/achievement.o src/profile.o src/notification.o src/sound.o src/overlay.o

INCDIR = include ../shared/include
LIBDIR =

CFLAGS = -O2 -G0 -Wall -Wextra -Wpedantic
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

BUILD_PRX = 1
USE_KERNEL_LIBS = 1

PRX_EXPORTS = exports.exp

LIBS = -lpspaudio
LDFLAGS =

PSPSDK := $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
