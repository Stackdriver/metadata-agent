#!/bin/sh

DEFAULT_VERSION="0.0.1.git"

if [ -d .git ]; then
	VERSION="`git describe --dirty=+ --abbrev=7 2> /dev/null | grep metadata-agent | sed -e 's/^metadata-agent-//' -e 's/-/./g'`"
fi

if test -z "$VERSION"; then
	VERSION="$DEFAULT_VERSION"
fi

printf "%s" "$VERSION"
