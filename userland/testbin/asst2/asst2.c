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
char teststr2[] = "The five boxing wizards jump quickly";
char buf[MAX_BUF];
int failed_tests = 0;

int
main(int argc, char * argv[])
{
        int iter;
        int fd, newfd, r, i, j , k;
        (void) argc;
        (void) argv;

        printf("\n**********\n* File Tester\n");

        snprintf(buf, MAX_BUF, "**********\n* write() works for stdout\n");
        write(1, buf, strlen(buf));
        snprintf(buf, MAX_BUF, "**********\n* write() works for stderr\n");
        write(2, buf, strlen(buf));

        /* testing open() with fake device prefix */
        printf("**********\n* opening new file \"fake_dev:\"\n");
        fd = open("fake_def:", O_WRONLY );
        printf("* open() got fd %d\n", fd);
        if (fd < 0) {
                printf("* error thrown as expected: %s\n", strerror(errno));
        } else {
                printf("ERROR opening of file for fake device should have failed\n");
                failed_tests++;
        }

        /* testing open() with fake directory */
        printf("**********\n* opening new file \"fake/test.file\"\n");
        fd = open("/fake/test.file", O_RDWR | O_CREAT );
        printf("* open() got fd %d\n", fd);
        if (fd < 0) {
                printf("* error thrown as expected: %s\n", strerror(errno));
        } else {
                printf("ERROR opening of file inside fake directory should have failed\n");
                failed_tests++;
        }

        /* testing open() with non-existant file */
        printf("**********\n* opening new file \"nonexistent.file\"\n");
        fd = open("nonexistent.file", O_RDWR );
        printf("* open() got fd %d\n", fd);
        if (fd < 0) {
                printf("* error thrown as expected: %s\n", strerror(errno));
        } else {
                printf("ERROR opening of nonexistent file should have failed\n");
                failed_tests++;
        }

        /* testing open() with invalid flags */
        printf("**********\n* opening new file \"test.file\" with invalid flags\n");
        fd = open("test.file", 255 );
        printf("* open() got fd %d\n", fd);
        if (fd < 0) {
                printf("* error thrown as expected: %s\n", strerror(errno));
        } else {
                printf("ERROR opening with invalid flags should have failed\n");
                failed_tests++;
        }

        /* testing open() with test.file */
        printf("**********\n* opening new file \"test.file\"\n");
        fd = open("test.file", O_RDWR | O_CREAT );
        printf("* open() got fd %d\n", fd);
        if (fd < 0) {
                printf("ERROR opening file: %s\n", strerror(errno));
                failed_tests++;
        }

        /* testing write() on test.file */
        printf("* writing test string\n");
        r = write(fd, teststr, strlen(teststr));
        printf("* wrote %d bytes\n", r);
        if (r < 0) {
                printf("ERROR writing file: %s\n", strerror(errno));
                failed_tests++;
        }

        printf("* writing test string again\n");
        r = write(fd, teststr, strlen(teststr));
        printf("* wrote %d bytes\n", r);
        if (r < 0) {
                printf("ERROR writing file: %s\n", strerror(errno));
                failed_tests++;
        }

        /* testing close() on file */
        printf("* closing file\n");
        close(fd);

        /* test system open file limit */
        printf("**********\n* open max number of files\nFD=");
        for (iter = 0; iter < 126; iter++) {
                fd = open("test.file", O_RDONLY);
                printf("%d..", fd);
                if (fd < 0) {
                        printf("\nERROR limit should not have been reached yet\n");
                        failed_tests++;
                        break;
                }
        }
        printf("DONE\n");
        printf("* open one more to surpass system limit\n");
        fd = open("test.file", O_RDONLY);
        if (fd < 0) {
                printf("* error thrown as expected: %s\n", strerror(errno));
        } else {
                printf("ERROR opening too many files should have failed\n");
                failed_tests++;
        }
        printf("* close all files\nFD=");
        for (fd = 0; fd < 128; fd++) {
                if (fd != 1 && fd != 2) {
                        printf("%d..", fd);
                        close(fd);
                }
        }
        printf("DONE\n");

        /* testing open() on existing file with O_EXCL */
        /*printf("**********\n* opening new file \"test.file\" which already exists\n");
        fd = open("test.file", O_RDWR | O_EXCL );
        printf("* open() got fd %d\n", fd);
        if (fd < 0) {
                printf("* error thrown as expected: %s\n", strerror(errno));
        } else {
                printf("ERROR opening of previously created file with O_EXCL flag should have failed\n");
                failed_tests++;
        }*/

        /* testing open() on existing file */
        printf("**********\n* opening old file \"test.file\"\n");
        fd = open("test.file", O_RDONLY);
        printf("* open() got fd %d\n", fd);
        if (fd < 0) {
                printf("ERROR opening file: %s\n", strerror(errno));
                failed_tests++;
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
                failed_tests++;
        }
        k = j = 0;
        r = strlen(teststr);
        do {
                if (buf[k] != teststr[j]) {
                        printf("ERROR file contents mismatch\n");
                        failed_tests++;
                }
                k++;
                j = k % r;
        } while (k < i);
        printf("* file content okay\n");

        /* lseek to out of bounds */
        printf("**********\n* testing lseek to out of bounds with SEEK_SET\n");
        r = lseek(fd, -1, SEEK_SET);
        if (r < 0) {
                printf("* error thrown as expected: : %s\n", strerror(errno));
        } else {
                printf("ERROR lseek should have failed on seek to out of bounds location\n");
                failed_tests++;
        }

        /* lseek SEEK_SET test */
        printf("**********\n* testing lseek with SEEK_SET\n");
        r = lseek(fd, 5, SEEK_SET);
        if (r < 0) {
                printf("ERROR lseek: %s\n", strerror(errno));
                failed_tests++;
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
                failed_tests++;
        }
        /* file position = 15 after read */

        k = 0;
        j = 5;
        r = strlen(teststr);
        do {
                if (buf[k] != teststr[j]) {
                        printf("ERROR  file contents mismatch\n");
                        failed_tests++;
                }
                k++;
                j = (k + 5)% r;
        } while (k < 5);

        printf("* file lseek SEEK_SET okay\n");

        /* dup2 on test.file */
        printf("**********\n* testing dup2 on test.file\n");
        newfd = dup2(fd, 3);
        if (newfd < 0) {
                printf("ERROR dup2: %s\n", strerror(errno));
                failed_tests++;
                exit(1);
        } else {
                printf("dup 2 successfully duplicated file at new fd: %d\n", newfd);
        }

        /* lseek SEEK_CUR test */
        printf("**********\n* testing lseek with SEEK_CUR\n");
        printf("* seeking backward from current position\n");
        r = lseek(fd, -5, SEEK_CUR);
        if (r < 0) {
                printf("ERROR lseek: %s\n", strerror(errno));
                failed_tests++;
        } else {
                printf("* file position is %d\n", r);
                printf("* expected is 10\n");
                if (r != 10) {
                        printf("ERROR lseek output is wrong\n");
                        failed_tests++;
                }
        }
        printf("* seeking forward from current position using duplicate fd\n");
        r = lseek(newfd, 17, SEEK_CUR);
        if (r < 0) {
                printf("ERROR lseek: %s\n", strerror(errno));
                exit(1);
        } else {
                printf("* file position is %d\n", r);
                printf("* expected is 27\n");
                if (r != 27) {
                        printf("ERROR lseek output is wrong\n");
                        failed_tests++;
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
                failed_tests++;
        }

        k = 0;
        j = 27;
        r = strlen(teststr);
        do {
                if (buf[k] != teststr[j]) {
                        printf("ERROR  file contents mismatch\n");
                        failed_tests++;
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
                failed_tests++;
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
                failed_tests++;
        }

        k = 0;
        j = 6;
        r = strlen(teststr);
        do {
                if (buf[k] != teststr[j]) {
                        printf("ERROR  file contents mismatch\n");
                        failed_tests++;
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
                failed_tests++;
        } else {
                printf("* lseek failed as expected and produced error: %s\n", strerror(errno));
        }

        /* lseek test on stderr should fail */
        printf("**********\n* testing lseek on stderr\n");
        r = lseek(1, 5, SEEK_SET);
        if (r != -1) {
                printf("ERROR lseek did not produce error\n");
                failed_tests++;
        } else {
                printf("* lseek failed as expected and produced error: %s\n", strerror(errno));
        }

        /* ensure duplicate is still open after original is closed. */
        printf("* test duplicate persistence after removing original\n");
        printf("* closing original file (fd = %d)\n", fd);
        close(fd);

        /* attempt writing to duplicate after original is closed. */
        printf("* writing test string to end duplicate\n");
        lseek(newfd, 0, SEEK_END);
        r = write(newfd, teststr, strlen(teststr));
        printf("* wrote %d bytes\n", r);
        if (r < 0) {
                printf("ERROR writing file: %s\n", strerror(errno));
                failed_tests++;
        }

        /* open a second file and write to it */
        printf("* open a second file and write test string2\n");
        fd = open("test2.file", O_RDWR | O_CREAT );
        printf("* open() got fd %d\n", fd);
        if (fd < 0) {
                printf("ERROR opening file: %s\n", strerror(errno));
                failed_tests++;
        }
        r = write(fd, teststr2, strlen(teststr2));
        printf("* wrote %d bytes\n", r);
        if (r < 0) {
                printf("ERROR writing file: %s\n", strerror(errno));
                failed_tests++;
        }
        r = write(fd, teststr2, strlen(teststr2));
        printf("* wrote %d bytes\n", r);
        if (r < 0) {
                printf("ERROR writing file: %s\n", strerror(errno));
                failed_tests++;
        }

        /* overwrite duplicate to a new file descriptor that already contains open file */
        /* dup2 on test.file */
        printf("**********\n* testing dup2 on test2.file\n");
        newfd = dup2(fd, 3);
        if (newfd < 0) {
                printf("ERROR dup2: %s\n", strerror(errno));
                failed_tests++;
                exit(1);
        } else {
                printf("dup 2 successfully duplicated file at new fd: %d\n", newfd);
        }

        /* lseek SEEK_CUR test */
        printf("**********\n* testing lseek with SEEK_CUR\n");
        printf("* seeking backward from current position\n");
        r = lseek(fd, -30, SEEK_CUR);
        if (r < 0) {
                printf("ERROR lseek: %s\n", strerror(errno));
                failed_tests++;
        } else {
                printf("* file position is %d\n", r);
                printf("* expected is 42\n");
                if (r != 42) {
                        printf("ERROR lseek output is wrong\n");
                        failed_tests++;
                }
        }
        printf("* seeking forward from current position using duplicate fd\n");
        r = lseek(newfd, 10, SEEK_CUR);
        if (r < 0) {
                printf("ERROR lseek: %s\n", strerror(errno));
                exit(1);
        } else {
                printf("* file position is %d\n", r);
                printf("* expected is 52\n");
                if (r != 52) {
                        printf("ERROR lseek output is wrong\n");
                        failed_tests++;
                }
        }

        printf("* reading 10 bytes of duplicate file into buffer \n");
        i = 0;
        do  {
                printf("* attempting read of %d bytes from duplicate file\n", 10 - i );
                r = read(newfd, &buf[i], 10 - i);
                printf("* read %d bytes\n", r);
                i += r;
        } while (i < 10 && r > 0);
        printf("* reading complete\n");
        if (r < 0) {
                printf("ERROR reading file: %s\n", strerror(errno));
                failed_tests++;
        }

        k = 0;
        r = strlen(teststr2);
        j = 52%r;
        do {
                if (buf[k] != teststr2[j]) {
                        printf("ERROR  file contents mismatch\n");
                        failed_tests++;
                }
                k++;
                j = (k + 16)% r;
        } while (k < 5);

        printf("* duplicate overwrite test okay\n");
        printf("* closing file\n");
        close(fd);
        close(newfd);

        printf("**********\n* one last test to check that global oft was cleared \n");

        /* test system open file limit */
        for (iter = 0; iter < 126; iter++) {
                fd = open("test.file", O_RDONLY);
                if (fd < 0) {
                        printf("\nERROR limit should not have been reached yet\n");
                        failed_tests++;
                        break;
                }
        }
        fd = open("test.file", O_RDONLY);
        if (fd < 0) {
                printf("* error thrown as expected: %s\n", strerror(errno));
        } else {
                printf("ERROR opening too many files should have failed\n");
                failed_tests++;
        }
        for (fd = 0; fd < 128; fd++) {
                if (fd != 1 && fd != 2) {
                        close(fd);
                }
        }

        if (failed_tests) {
                printf("* FAILED TESTS %d\n", failed_tests);
        } else {
                printf("* PASSED ALL TESTS\n");
        }
        printf("* testing complete\n\n");

        return 0;
}


