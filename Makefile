build-image: Dockerfile.build
	docker build --platform linux/amd64 -t clambda/build1 -f Dockerfile.build .
runtime-image: Dockerfile.runtime
	docker build --platform linux/amd64 -t clambda/runtime1 -f Dockerfile.runtime .

build:
	docker run --platform linux/amd64 -it -v .:/opt/clambda clambda/build1

bootstrap: clambda.c clambda.h json.h
	cc -o bootstrap clambda.c $$(curl-config --libs) -ldl

clean:
	rm bootstrap
	rm *.o
