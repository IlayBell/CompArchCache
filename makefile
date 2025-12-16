
cacheSim: cacheSim.cpp cache.cpp
	g++ -o cacheSim cacheSim.cpp cache.cpp

.PHONY: clean
clean:
	rm -f *.o
	rm -f cacheSim
