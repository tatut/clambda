#!/bin/sh

docker run -it -v .:/opt/clambda -e HANDLER=$1 clambda/build
