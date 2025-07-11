
all:
	@echo "Usage:"
	@echo "make toxml"
	@echo "make fromxml"
	@echo "make linksources"

toxml:
	xmq project.xmq to-xml > .project
	xmq cproject.xmq to-xml > .cproject

fromxml:
	xmq .project > project.xmq
	xmq .cproject > cproject.xmq

linksources:
	rm -rf src
	mkdir -p src/parts
	for i in ../../src/main/c/parts/* ; do (cd src/parts; ln -s ../../$$i) ; done
	rm -f src/parts/yaep.c src/parts/yaep.h
	for i in ../../src/main/c/yaep/src/* ; do (cd src/parts; ln -s ../../$$i) ; done
	cd src; ln -s ../../../src/main/c/xmq.c
	cd src; ln -s ../../../src/main/c/xmq-cli.c
	cd src; ln -s ../../../src/main/c/xmq.h
	cd src/parts; rm testinternals.c
