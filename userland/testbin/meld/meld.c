/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * filetest.c
 *
 * 	Tests the filesystem by opening, writing to and reading from a
 * 	user specified file.
 *
 * This should run (on SFS) even before the file system assignment is started.
 * It should also continue to work once said assignment is complete.
 * It will not run fully on emufs, because emufs does not support remove().
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <err.h>

int
main(int argc, char *argv[])
{
	static char writebuf[40] = "Twiddle dee dee, Twiddle dum dum.......\n";
	static char writebuf2[40] = "This is going to look bad when melded.\n";
	static char readbuf[81];

	const char *file;
	const char *file2;
	const char *file3;
	int fd, rv, fd2, rv2, fd3, rv3;

	if (argc == 0) {
		/*warnx("No arguments - running on \"testfile\"");*/
		file = "testfile";
	}
	else if (argc != 3) {
		errx(1, "Usage: filetest <filename1> < filename2> <filename3>");
	}
	else {
		file = argv[1];
		file2 = argv[2];
		file3 = argv[3];
	}

	fd = open(file, O_WRONLY|O_CREAT|O_TRUNC, 0664);
	if (fd<0) {
		err(1, "%s: open for write", file);
	}

	rv = write(fd, writebuf, 40);
	if (rv<0) {
		err(1, "%s: write", file);
	}

	rv = close(fd);
	if (rv<0) {
		err(1, "%s: close (1st time)", file);
	}

	fd3 = open(file2, O_WRONLY|O_CREAT|O_TRUNC, 0664);
	if (fd<0) {
		err(1, "%s: open for write", file);
	}


	rv = write(fd2, writebuf2, 40);
	if (rv<0) {
		err(1, "%s: write", file);
	}

	rv = close(fd2);
	if (rv<0) {
		err(1, "%s: close (1st time)", file);
	}

	fd = open(file, O_RDONLY);
	if (fd<0) {
		err(1, "%s: open for read", file);
	}

	rv = read(fd, readbuf, 40);
	if (rv<0) {
		err(1, "%s: read", file);
	}

	rv = close(fd);
	printf("Closing fd=%d retval=%d.\n", fd, rv);
	if (rv<0) {
		err(1, "%s: close (2nd time)", file);
	}
	/* ensure null termination */
	readbuf[40] = 0;

	if (strcmp(readbuf, writebuf)) {
		errx(1, "Buffer data mismatch!");
	}

	printf("Meld section of the test!\n");

	struct meldargs * meldarg1;
	struct meldargs * meldarg2;
	struct meldargs * meldarg3;

	meldarg1->filename = file1;
	meldarg2->filename = file2;
	meldarg3->filename = file3;

	meldarg1->filehandle = fd;
	meldarg2->filehandle = fd2;
	meldarg3->filehandle = fd3;

	meldarg1->size = 40;
	meldarg2->size = 40;
	meldarg3->size = 81;

	rv = meld(meldarg1, meldarg2, meldarg3);
	if(rv != 0)
		errx(1, "Meld exited with failure!");

/*	rv = remove(file);
	if (rv<0) {
		err(1, "%s: remove", file);
	} */
	printf("Passed filetest.\n");
	return 0;
}
