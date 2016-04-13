TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt
CONFIG += c++11

LIBS += -lpthread -lboost_program_options

QMAKE_CXXFLAGS += -std=c++11 -Wall -Wextra -pedantic

SOURCES += main.cpp \
    FastLineReader.cpp \
    EventParser.cpp \
    Data.cpp \
    Options.cpp \
    SimpleStats.cpp \
    workloadadaptation.cpp

HEADERS += \
    FastLineReader.h \
    EventParser.h \
    Data.h \
    Options.h \
    SimpleStats.h \
    Utils.h \
    workloadadaptation.h

