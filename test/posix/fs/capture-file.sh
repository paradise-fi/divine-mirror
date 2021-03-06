# TAGS: capture
. lib/testcase

cat > test.c <<EOF
/* VERIFY_OPTS: --capture capture/file:follow:/file.txt */
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>

int main() {

    int fd = open( "file.txt", O_RDONLY );
    assert( fd >= 0 );
    assert( close( fd ) == 0 );

    return 0;
}
EOF

mkdir capture/
touch capture/file

verify test.c
