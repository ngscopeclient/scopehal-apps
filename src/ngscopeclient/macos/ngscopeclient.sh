#!/bin/sh

BUNDLE="`echo "$0" | sed -e 's/\/Contents\/MacOS\/ngscopeclient//'`"
RESOURCES="$BUNDLE/Contents/Resources"

export "PATH=$RESOURCES/bin:$PATH"

exec "$RESOURCES/bin/ngscopeclient"

#eof

