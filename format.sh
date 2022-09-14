#!/bin/bash

BASE_DIR=$(cd $(dirname $0);pwd)
cd ${BASE_DIR}

function format() {
  filename="$1"
  echo $filename
  clang-format -i -style=file "${filename}"
}

for filename in `find ./src ./include ./examples \( -name \*.c -o -name \*.h \)`; do
  format ${filename}
done
