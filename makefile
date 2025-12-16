cacheSim: cacheSim.cpp cache.cpp
	g++ -std=c++11 -o cacheSim cacheSim.cpp cache.cpp

.PHONY: clean
clean:
	rm -f *.o
	rm -f cacheSim
