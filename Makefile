all: 
	mkdir -p build && cd build && cmake .. && make && cd -

install:
	make -C build install

clean:
	rm -rf build/src 

distclean:
	rm -rf build 
