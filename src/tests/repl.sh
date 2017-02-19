#!/bin/sh -ex

LIP=$1

test "$(echo '(print (identity true))' | $LIP)" = "true"
test "$($LIP -i -e '(print (identity true))')" = "true"
test "$(echo '(print (identity true))' | $LIP -i)" = "true"

echo '(print "Hello world")' | $LIP --compile bin/hello_world.lipc
test "$($LIP bin/hello_world.lipc)" = '"Hello world"'

echo '(print "Hello world again")' > bin/temp.lip
$LIP --compile bin/temp.lipc bin/temp.lip
test "$($LIP bin/temp.lipc)" = '"Hello world again"'

$LIP --compile bin/temp --standalone bin/temp.lip
test "$(bin/temp)" = '"Hello world again"'
