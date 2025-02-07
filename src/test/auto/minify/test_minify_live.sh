#!/bin/bash
# test_minify_live.sh
# We minify some JS text and then run it in Node, both before and after minification.
# Examine Node's output to validate that minification didn't change the script's behavior.

cat - >jspre-tmp.js <<'EOF'

test1("add and mlt", 2+3*4);
test1("mlt and add", 2*3+4);
test1("add first", (2+3)*4);
test1("also add first", 2*(3+4));

//TODO Exhaustive validation of operator precedence.

EOF

#--- Remainder is boilerplate, shouldn't need to change. ---

cat - >jspfx-tmp.js <<'EOF'
const testResults = [];
function test1(k, v) {
  const p = testResults.findIndex(r => r.k === k);
  if (p >= 0) {
    if (v !== testResults[p].v) {
      console.log(`EGG_TEST FAIL test_minify_live ${k} ${testResults[p].v} != ${v}`);
    } else {
      console.log(`EGG_TEST PASS test_minify_live ${k}`);
    }
    testResults.splice(p, 1);
  } else {
    testResults.push({ k, v });
  }
}
EOF

cat - >jssfx-tmp.js <<'EOF'
if (testResults.length) {
  console.log(`EGG_TEST FAIL test_minify_live ${testResults.length} dangling test results`);
} else {
  console.log(`EGG_TEST PASS test_minify_live`);
}
EOF

out/eggdev minify -ojsmin-tmp.js jspre-tmp.js || exit 1
cat jspfx-tmp.js jspre-tmp.js jsmin-tmp.js jssfx-tmp.js >jscat-tmp.js

node jscat-tmp.js

rm *-tmp.js
