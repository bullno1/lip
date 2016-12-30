#!/bin/sh -ex

LIP=$1

test "$(echo '(print (identity true))' | $LIP)" = "true"
test "$($LIP -i -e '(print (identity true))')" = "true"
test "$(echo '(print (identity true))' | $LIP -i)" = "true"
