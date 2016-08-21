TOPDIR := $(shell pwd)
TOPRPMDIR := $(TOPDIR)/build/rpmbuild
all: 
	mkdir -p build && cd build && \
	cmake .. && make && cd -

debug:
	mkdir -p build && cd build && \
	cmake -DCMAKE_BUILD_TYPE=Debug .. && make && cd -

install:
	make -C build install

.PHONY:release
release: all
	mkdir -p build/rpmbuild/{BUILD,RPMS,SOURCES,SPECS,SRPMS}
	rpmbuild --quiet -bb $(TOPDIR)/scripts/csperf.spec --define '_topdir $(TOPRPMDIR)'\
		--define '_mypath $(TOPDIR)'\
		--define '_builddir $(TOPRPMDIR)/BUILD'\
		--define '_specdir $(TOPDIR)/SPECS'\
		--define '_rpmdir $(TOPRPMDIR)/RPMS'\
		--define '_sourcedir $(TOPRPMDIR)/SOURCES'\
		--define '_specdir $(TOPRPMDIR)/SPECS'\
		--define '_srcrpmdir $(TOPRPMDIR)/SRPMS'\
		$(TOPDIR)/scripts/csperf.spec && \
		cp $(TOPRPMDIR)/RPMS/*/*.rpm $(TOPDIR)/build/

clean:
	rm -rf build/src 
	rm -rf build; rm -rf core* 
	rm -rf /tmp/csperf_info.log
