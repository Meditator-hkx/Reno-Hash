CC := g++ 
LDLIBS := -lpthread -libverbs
CFLAGS := -Wall -g -m64
APPS := reno

all: $(APPS)

reno: main.cpp exp.cpp api.cpp storage.cpp config.cpp hash.cpp siphash.cpp tcp.cpp ib.cpp
	$(CC) -std=c++11 ${CFLAGS} -o $@ $^ ${LDLIBS}

clean:
	rm -f *.o $(APPS)
