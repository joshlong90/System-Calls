#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <err.h>
#include <errno.h>

#define MAX_BUF 500
char teststr[] = "The quick brown fox jumped over the lazy dog.";
char buf[MAX_BUF];

int
main(int argc, char * argv[])
{
        int fd, r, i, j , k;
        (void) argc;
        (void) argv;

        printf("\n**********\n* File Tester\n");

        snprintf(buf, MAX_BUF, "**********\n* write() works for stdout\n");
        write(1, buf, strlen(buf));
        snprintf(buf, MAX_BUF, "**********\n* write() works for stderr\n");
        write(2, buf, strlen(buf));

        /* testing open() with test.file */
        printf("**********\n* opening new file \"test.file\"\n");
        fd = open("test.file", O_RDWR | O_CREAT );
        printf("* open() got fd %d\n", fd);
        if (fd < 0) {
                printf("ERROR opening file: %s\n", strerror(errno));
                exit(1);
        }

        /* testing write() on test.file */
        printf("* writing test string\n");
        r = write(fd, teststr, strlen(teststr));
        printf("* wrote %d bytes\n", r);
        if (r < 0) {
                printf("ERROR writing file: %s\n", strerror(errno));
                exit(1);
        }

        printf("* writing test string again\n");
        r = write(fd, teststr, strlen(teststr));
        printf("* wrote %d bytes\n", r);
        if (r < 0) {
                printf("ERROR writing file: %s\n", strerror(errno));
                exit(1);
        }

        /* testing close() on file */
        printf("* closing file\n");
        close(fd);

        /* testing open() on existing file */
        printf("**********\n* opening old file \"test.file\"\n");
        fd = open("test.file", O_RDONLY);
        printf("* open() got fd %d\n", fd);
        if (fd < 0) {
                printf("ERROR opening file: %s\n", strerror(errno));
                exit(1);
        }

        /* testing read() on entire file into buffer */
        printf("* reading entire file into buffer \n");
        i = 0;
        do  {
                printf("* attempting read of %d bytes\n", MAX_BUF -i);
                r = read(fd, &buf[i], MAX_BUF - i);
                printf("* read %d bytes\n", r);
                i += r;
        } while (i < MAX_BUF && r > 0);

        printf("* reading complete\n");
        if (r < 0) {
                printf("ERROR reading file: %s\n", strerror(errno));
                exit(1);
        }
        k = j = 0;
        r = strlen(teststr);
        do {
                if (buf[k] != teststr[j]) {
                        printf("ERROR  file contents mismatch\n");
                        exit(1);
                }
                k++;
                j = k % r;
        } while (k < i);
        printf("* file content okay\n");

        /* lseek SEEK_SET test */
        printf("**********\n* testing lseek with SEEK_SET\n");
        r = lseek(fd, 5, SEEK_SET);
        if (r < 0) {
                printf("ERROR lseek: %s\n", strerror(errno));
                exit(1);
        }
        /* file position = 5 after lseek */

        printf("* reading 10 bytes of file into buffer \n");
        i = 0;
        do  {
                printf("* attempting read of %d bytes\n", 10 - i );
                r = read(fd, &buf[i], 10 - i);
                printf("* read %d bytes\n", r);
                i += r;
        } while (i < 10 && r > 0);
        printf("* reading complete\n");
        if (r < 0) {
                printf("ERROR reading file: %s\n", strerror(errno));
                exit(1);
        }
        /* file position = 15 after read */

        k = 0;
        j = 5;
        r = strlen(teststr);
        do {
                if (buf[k] != teststr[j]) {
                        printf("ERROR  file contents mismatch\n");
                        exit(1);
                }
                k++;
                j = (k + 5)% r;
        } while (k < 5);

        printf("* file lseek SEEK_SET okay\n");

        /* lseek SEEK_CUR test */
        printf("**********\n* testing lseek with SEEK_CUR\n");
        printf("* seeking backward from current position\n");
        r = lseek(fd, -5, SEEK_CUR);
        if (r < 0) {
                printf("ERROR lseek: %s\n", strerror(errno));
                exit(1);
        } else {
                printf("* file position is %d\n", r);
                printf("* expected is 10\n");
                if (r != 10) {
                        printf("ERROR lseek output is wrong\n");
                        exit(1);
                }
        }
        printf("* seeking forward from current position\n");
        r = lseek(fd, 17, SEEK_CUR);
        if (r < 0) {
                printf("ERROR lseek: %s\n", strerror(errno));
                exit(1);
        } else {
                printf("* file position is %d\n", r);
                printf("* expected is 27\n");
                if (r != 27) {
                        printf("ERROR lseek output is wrong\n");
                        exit(1);
                }
        }

        printf("* reading 10 bytes of file into buffer \n");
        i = 0;
        do  {
                printf("* attempting read of %d bytes\n", 10 - i );
                r = read(fd, &buf[i], 10 - i);
                printf("* read %d bytes\n", r);
                i += r;
        } while (i < 10 && r > 0);
        printf("* reading complete\n");
        if (r < 0) {
                printf("ERROR reading file: %s\n", strerror(errno));
                exit(1);
        }

        k = 0;
        j = 27;
        r = strlen(teststr);
        do {
                if (buf[k] != teststr[j]) {
                        printf("ERROR  file contents mismatch\n");
                        exit(1);
                }
                k++;
                j = (k + 27)% r;
        } while (k < 5);

        printf("* file lseek SEEK_CUR okay\n");

        /* lseek SEEK_END test */
        printf("**********\n* testing lseek with SEEK_END\n");
        r = lseek(fd, -39, SEEK_END);
        if (r < 0) {
                printf("ERROR lseek: %s\n", strerror(errno));
                exit(1);
        }

        printf("* reading 10 bytes of file into buffer \n");
        i = 0;
        do  {
                printf("* attempting read of %d bytes\n", 10 - i );
                r = read(fd, &buf[i], 10 - i);
                printf("* read %d bytes\n", r);
                i += r;
        } while (i < 10 && r > 0);
        printf("* reading complete\n");
        if (r < 0) {
                printf("ERROR reading file: %s\n", strerror(errno));
                exit(1);
        }

        k = 0;
        j = 6;
        r = strlen(teststr);
        do {
                if (buf[k] != teststr[j]) {
                        printf("ERROR  file contents mismatch\n");
                        exit(1);
                }
                k++;
                j = (k + 6)% r;
        } while (k < 5);

        printf("* file lseek SEEK_END okay\n");
        printf("* closing file\n");
        close(fd);

        /* lseek test on stdout should fail */
        printf("**********\n* testing lseek on stdout\n");
        r = lseek(1, 5, SEEK_SET);
        if (r != -1) {
                printf("ERROR lseek did not produce error\n");
                exit(1);
        } else {
                printf("* lseek failed as expected and produced error: %s\n", strerror(errno));
        }
        printf("* lseek on invalid fd successful\n");

        /* lseek test on stderr should fail */
        printf("**********\n* testing lseek on stderr\n");
        r = lseek(1, 5, SEEK_SET);
        if (r != -1) {
                printf("ERROR lseek did not produce error\n");
                exit(1);
        } else {
                printf("* lseek failed as expected and produced error: %s\n", strerror(errno));
        }
        printf("* lseek on invalid fd successful\n");

        printf("* testing complete\n\n");

        return 0;
}


