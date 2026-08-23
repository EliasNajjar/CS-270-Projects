#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include "amap.h"
#include "auxiliary.h"

/* reducer - reads (word,count) pairs from different scanners via pipes,
 *  uses the map to sum them up.
 * When the pipe is at EOF, use amap_getnext() to dump them one by one
 * into the pipe the driver reads from.
 * The pipes handle flow control and indicate when we finish, so we don't
 * need any other synchronization.
 */

void reduce(int numprocs, int me, amap_t *map,
	    pipe_t *reducepipes, pipe_t *driverpipes) {

  /******* YOUR CODE HERE ****/

  /* first close all pipe ends that we won't be using */
  for (int i = 0; i < numprocs; i++) {
    if (i != me) { // close all pipes that aren't me
      close(reducepipes[i].readfd);
      close(reducepipes[i].writefd);
      close(driverpipes[i].readfd);
      close(driverpipes[i].writefd);
    }
  }
  close(reducepipes[me].writefd); // close me pipes we won't use
  close(driverpipes[me].readfd);

  /* Then read from "my" reducepipe, summing pairs in the map.
   * Note: map is empty at this point.
   * Hint: use  readpair() in auxiliary.c.
   */
  char key[MAXSTRING];
  int c;
  while (readpair(reducepipes[me].readfd, key, &c) > 0) {
    amap_incr(map, key, c);
  }

  /* Now dump the map counts to the driver pipe.
   * Hint: amap_getnext() returns pairs one-at-a-time, in sorted order.
   * Hit: Use writepair() in auxiliary.c.
   */
  while (amap_getnext(map, key, &c) > 0) {
    writepair(driverpipes[me].writefd, key, &c);
  }

  /* Finally, close write end of driverpipe[me], to tell driver I'm done. */
  close(driverpipes[me].writefd);

  exit(0);
}
