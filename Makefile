all: 
	mkdir -p build && cd build && \
	cmake .. && make && cd -

debug:
	mkdir -p build && cd build && \
	cmake -DCMAKE_BUILD_TYPE=Debug .. && make && cd -

install:
	make -C build install

clean:
	rm -rf build/src 

distclean:
	rm -rf build 
