TEMPLATE = app
CONFIG += console
CONFIG -= qt

unix {
    CONFIG += link_pkgconfig
    PKGCONFIG += libmutomIB-0.3.5
    QMAKE_CXXFLAGS += -fopenmp
    LIBS += -fopenmp
}

SOURCES += IBTrackDump.cpp

target.path = /usr/local/bin

INSTALLS += target