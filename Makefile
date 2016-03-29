PROG = multi-get
SRC = concurrent_multiget.cpp
CFLAGS = -std=c++11 -pthread
OPT_LEVEL = -O2
LIBS = -lcurl

$(PROG): $(SRC)
	g++ $(CFLAGS) $(OPT_LEVEL) $(SRC) -o $(PROG) $(LIBS)

