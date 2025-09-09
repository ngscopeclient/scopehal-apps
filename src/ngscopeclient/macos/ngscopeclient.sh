#!/bin/sh

BUNDLE="`echo "$0" | sed -e 's/\/Contents\/MacOS\/ngscopeclient//'`"
RESOURCES="$BUNDLE/Contents/Resources"

export "PATH=$RESOURCES/bin:$PATH"
#HACK: no clue why install_name_tool isn't fixing this...
export DYLD_LIBRARY_PATH="$RESOURCES/lib:$RESOURCES/Frameworks:${DYLD_LIBRARY_PATH}"

exec "$RESOURCES/bin/ngscopeclient" $*

#eof

