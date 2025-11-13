#!/bin/sh

cd /opt/clambda
if [[ -n "$HANDLER" ]]; then
    echo "Building handler: $HANDLER"
    cc -fPIC -shared -o $HANDLER.o $HANDLER.c -lm
else
    echo "Building C lambda bootstrap"
    make bootstrap
fi
