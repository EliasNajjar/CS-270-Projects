#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>
#include "amap.h"
#include "auxiliary.h"


/* scanner.c - process directories/files, counting words in each
 *   regular text file.  Accumulate counts in the amap_t, then pass them
 *   along to reducers via the reducer pipes.
 * Each reducer collects counts for a portion of the alphabet.
 *   Which reducer pipe a (word,count) pair should be written to is
 *   determined by the first  letter of word, using the "whichpipe" global.
 * The definition of a "word" is a contiguous sequence of alphabetic
 *   characters - no numbers, whitespace, or punctuation.
 */

#define FTYPE_REG 1
#define FTYPE_DIR 2
#define FTYPE_SKIP 3

#define SAMPLESIZE 16
#define NAMESIZE 1024

/* Map of characters to reducer pipe #s. Init'ed in driver once for all. */
extern int whichpipe[ALPHABETLEN];

/* what kind of file is this?  Return value indicates type via constants above.
 * If it's a text file to be processed, the file descriptor is passed via
 * the "fd" out parameter.
 */
static int check_type(char  *name, int *fd) {
  struct stat finfo;
  int rv, fildes;
  char buf[SAMPLESIZE];
  rv = stat(name,&finfo);
  if (rv < 0) {
    return FTYPE_SKIP;
  }
  if ((finfo.st_mode & S_IFMT) == S_IFREG) {
    fildes = open(name,O_RDONLY);
    if (fildes < 0)
      return FTYPE_SKIP;
    /* Read a few characters to try to get type */
    if ((rv=read(fildes,buf,SAMPLESIZE)) < 0)
      return FTYPE_SKIP;
    if ((buf[0] == 0x7f &&
	buf[1] == 'E' &&
	buf[2] == 'L' &&
	 buf[3] == 'F')  // executable
	||
	(buf[0] =='G' &&
	 buf[1] == 'I' &&
	 buf[2] == 'F')  // image
	||
	(buf[0] == 0x89 &&
	 buf[1] == 'P' &&
	 buf[2] == 'N' &&
	 buf[3] == 'G') // image
	||
	(buf[0] == '%' &&
	 buf[1] == 'P' &&
	 buf[2] == 'D' &&
	 buf[3] == 'F')) { // PDF
      close(fildes);
      return FTYPE_SKIP;
    }
    for (int i=0; i<rv; i++)
      if (!isascii(buf[i])) {
	close(fildes);
	return FTYPE_SKIP;
      }
    /* first rv bytes are all ASCII */
    *fd = fildes;
    return FTYPE_REG;
  } else if ((finfo.st_mode & S_IFMT) == S_IFDIR)
    return FTYPE_DIR;
  else return FTYPE_SKIP;  // who knows?
}

/* read from file, splitting input into alphabetic tokens */
static void count_words(FILE *f, amap_t *m) {
  char buf[MAXSTRING];
  int c, len;
  for (;;) {
    len = 0;
    c = getc(f);
    while (c != EOF && !isalpha(c)) // skip non-alpha chars
      c = getc(f);
    /* c==EOF || isalpha(c) */
    if (c==EOF)
      return;
    /* isalpha(c) */
    do  {
      buf[len++] = tolower(c);  // add alpha chars to the word
      c = getc(f);
    } while (isalpha(c) && len < MAXSTRING);
    buf[len] = 0; // terminate string
    amap_incr(m,buf,1); // add 1 to the count, insert if not there.
  }
}

/* Given a filename, figure out if it's a directory or text file.
 * If text file (base case), scan it, incrementing word counts as you go.
 * If directory, recurse.
 * Otherwise, ignore
 */
void process_file(char *name,  amap_t *m)  {
  /******** YOUR CODE  HERE ************/

  /* What kind of file is it?
   * Hint: use check_type()
   */
  int fd;
  int type = check_type(name, &fd);

  /* if FTYPE_REG: use  count_words().  Note arg types expected by it.
   *    Hint: fdopen().
   * Hint: account for what check_type() does with a regular file.
   */
  if (type == FTYPE_REG) {
    lseek(fd, 0, SEEK_SET); // seek start of file
    FILE* f = fdopen(fd, "r");
    count_words(f, m);
    fclose(f);
  }

  /* if FTYPE_DIR: use opendir  and readdir t0 get entries,
   *    and recursively process each.
   */
  else if (type == FTYPE_DIR) {
    DIR* d = opendir(name);
    struct dirent* file;
    while ((file = readdir(d)) != NULL) {
      if (strcmp(file->d_name, ".") != 0 && strcmp(file->d_name, "..") != 0) { // ignore self and parent
        char path[NAMESIZE];
        snprintf(path, sizeof(path), "%s/%s", name, file->d_name); // get full path
        process_file(path, m);
      }
    }
    closedir(d);
  }

  /* if anything else, ignore it. */
}

/*
 * Driver for the scanning process.
 * Process the files under the given name (process_file), recursively,
 * incrementing count in the map for each word found in a text file.
 * Then write the contents of the map to each reducer pipe, according to
 * the first letter of the word (using the writepipes map).
 * Then close all the write ends of the reducer pipes.
 * Parameters:
 *  nprocs = # of scanners/reducers/pipes
 *  map = sorted map of words to counts
 *  startname = file/dir to process
 *  reducepipes = array of pipe fds to get counts to reducers
 * THIS FUNCTION DOES NOT RETURN
 */
void scanner(int nprocs, amap_t *map, char *startname, pipe_t *reducepipes) {
  /************* YOUR CODE HERE *********************/

  for (int i = 0; i < nprocs; i++) {
    close(reducepipes[i].readfd);
  }

  /* first call process_file() on the startname. */
  process_file(startname, map);

  /* Then write each (string,count) pair in the map to the appropriate
   * reducer.
   * Hint: use amap_getnext().
   * Hint: whichpipe maps character #s ('a'=0, 'b'=1, etc.) to reduce  pipe #s.
   */
  char key[MAXSTRING];
  int c;
  while (amap_getnext(map, key, &c) > 0) {
    writepair(reducepipes[whichpipe[*key - 'a']].writefd, key, &c);
  }

  /* Then close my write end of each reduce pipe. */
  for (int i = 0; i < nprocs; i++) {
    close(reducepipes[i].writefd);
  }

  exit(0); // all done!
}
