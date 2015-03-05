TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt
CONFIG += c++11

LIBS += -lpthread -lboost_program_options

QMAKE_CXXFLAGS += -std=c++11 -Wall -Wextra -pedantic -Wcast-align -Wcast-qual -Wctor-dtor-privacy -Wdisabled-optimization -Wformat=2 -Winit-self -Wlogical-op -Wmissing-declarations -Wmissing-include-dirs -Wnoexcept -Wold-style-cast -Woverloaded-virtual -Wredundant-decls -Wshadow -Wsign-conversion -Wsign-promo -Wstrict-null-sentinel -Wstrict-overflow=5 -Wswitch-default -Wundef -Werror -Wno-unused

SOURCES += main.cpp \
    FastLineReader.cpp \
    EventParser.cpp \
    Data.cpp \
    Options.cpp \
    SimpleStats.cpp

include(deployment.pri)
qtcAddDeployment()

HEADERS += \
    FastLineReader.h \
    EventParser.h \
    Data.h \
    Options.h \
    SimpleStats.h

