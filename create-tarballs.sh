#!/usr/bin/env bash

#Loosely based on https://www.gnu.org/software/tar/manual/html_section/Reproducibility.html
#Drop the mtime attribute from the files since I was unable to reliably make that reproducible

set -e

export TZ=UTC0
export LANG=C

location="$(dirname -- "$( readlink -f -- "$0"; )";)"
cd "$location"

export NGSCOPECLIENT_PACKAGE_VERSION="$(git describe --always --tags)"
export NGSCOPECLIENT_PACKAGE_VERSION_LONG="$(git describe --always --tags --long)"
export SCOPEHAL_PACKAGE_VERSION="$(cd lib;git describe --always --tags --long)"
cat release-info.cmake.in | envsubst > release-info.cmake

git_file_list=$(git ls-files --recurse-submodules )

TARFLAGS="
  --sort=name --format=posix
  --pax-option=exthdr.name=%d/PaxHeaders/%f
  --pax-option=delete=atime,delete=ctime
  --mtime=0
  --numeric-owner --owner=0 --group=0
  --mode=go+u,go-w
"

git_file_list="$git_file_list release-info.cmake"

GZIPFLAGS="--no-name --best"

rm -Rf tarballs
mkdir "tarballs"

LC_ALL=C tar $TARFLAGS -cf - $git_file_list |
  gzip $GZIPFLAGS > "tarballs/ngscopeclient-$NGSCOPECLIENT_PACKAGE_VERSION.tar.gz"

cd tarballs
echo "" > ../digest.txt
for file in *; do
  sha256sum $file >> ../digest.txt
  ls -s $file >> ../digest.txt
  sha256sum $file > $file.sha256sum
done
mv ../digest.txt "ngscopeclient-$NGSCOPECLIENT_PACKAGE_VERSION-manifest.txt"
