/*
 * sasearch.c for libdivsufsort
 * Copyright (c) 2003-2008 Yuta Mori All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

# include "config.h"
# include <stdio.h>
# include <string.h>
# include <stdlib.h>
# include <memory.h>
# include <stddef.h>
# include <strings.h>
# include <sys/types.h>
# include <time.h>
# include <divsufsort.h>
# include "lfs.h"

#include <nanomsg/nn.h>
#include <nanomsg/reqrep.h>
#include <assert.h>
#include <fstream>
#include <iostream>
#include <stdlib.h>
#include <unistd.h>

using namespace std;

static
void
print_help(const char *progname, int status) {
  fprintf(stderr,
          "sadaemon, a daemon to search a suffix array file %s\n",
          divsufsort_version());
  fprintf(stderr, "usage: %s FILE SAFILE\n\n", progname);
  exit(status);
}

//bin/sa_daemon name_lexicon_uniq_mod sa_name_lexicon

int
main(int argc, const char *argv[]) {
  FILE *fp;
  const char *P;
  sauchar_t *T;
  saidx_t *SA;
  LFS_OFF_T n;
  size_t Psize;
  saidx_t size, left;
  clock_t start, finish;
  clock_t start2, finish2;

  if((argc == 1) ||
     (strcmp(argv[1], "-h") == 0) ||
     (strcmp(argv[1], "--help") == 0)) { print_help(argv[0], 0); }
  if(argc != 3) { print_help(argv[0], 1); }

  ofstream log_file;
  log_file.open("sa_daemon.log");

  pid_t pid, sid;
  pid = fork();
  if (pid < 0) {
    exit(EXIT_FAILURE);
  }
  if (pid > 0) {
    exit(EXIT_SUCCESS);
  }
  sid = setsid();
  if (sid < 0) {
    exit(EXIT_FAILURE);
  }
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);

  log_file << "SID: " << sid << " (to kill me type \"kill " << sid << "\" on command line or \"killall run_extract_daemon\" to kill all instances)" << endl;
  log_file << "Starting up...Please do not send any requests yet" << endl;

  string url = "ipc:///tmp/sa_daemon_test.ipc";
  int socket = nn_socket(AF_SP, NN_REP);
  assert(socket >= 0);
  assert(nn_bind(socket, url.c_str()) >= 0);

  start = clock();
  /* Open a file for reading. */
#if HAVE_FOPEN_S
  if(fopen_s(&fp, argv[1], "rb") != 0) {
#else
  if((fp = LFS_FOPEN(argv[1], "rb")) == NULL) {
#endif
    fprintf(stderr, "%s: Cannot open file `%s': ", argv[0], argv[1]);
    perror(NULL);
    exit(EXIT_FAILURE);
  }

  /* Get the file size. */
  if(LFS_FSEEK(fp, 0, SEEK_END) == 0) {
    n = LFS_FTELL(fp);
    rewind(fp);
    if(n < 0) {
      fprintf(stderr, "%s: Cannot ftell `%s': ", argv[0], argv[1]);
      perror(NULL);
      exit(EXIT_FAILURE);
    }
  } else {
    fprintf(stderr, "%s: Cannot fseek `%s': ", argv[0], argv[1]);
    perror(NULL);
    exit(EXIT_FAILURE);
  }

  /* Allocate 5n bytes of memory. */
  T = (sauchar_t *)malloc((size_t)n * sizeof(sauchar_t));
  SA = (saidx_t *)malloc((size_t)n * sizeof(saidx_t));
  if((T == NULL) || (SA == NULL)) {
    fprintf(stderr, "%s: Cannot allocate memory.\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  /* Read n bytes of data. */
  if(fread(T, sizeof(sauchar_t), (size_t)n, fp) != (size_t)n) {
    fprintf(stderr, "%s: %s `%s': ",
      argv[0],
      (ferror(fp) || !feof(fp)) ? "Cannot read from" : "Unexpected EOF in",
      argv[1]);
    perror(NULL);
    exit(EXIT_FAILURE);
  }
  fclose(fp);

  /* Open the SA file for reading. */
#if HAVE_FOPEN_S
  if(fopen_s(&fp, argv[2], "rb") != 0) {
#else
  if((fp = LFS_FOPEN(argv[2], "rb")) == NULL) {
#endif
    fprintf(stderr, "%s: Cannot open file `%s': ", argv[0], argv[2]);
    perror(NULL);
    exit(EXIT_FAILURE);
  }

  /* Read n * sizeof(saidx_t) bytes of data. */
  if(fread(SA, sizeof(saidx_t), (size_t)n, fp) != (size_t)n) {
    fprintf(stderr, "%s: %s `%s': ",
      argv[0],
      (ferror(fp) || !feof(fp)) ? "Cannot read from" : "Unexpected EOF in",
      argv[2]);
    perror(NULL);
    exit(EXIT_FAILURE);
  }
  fclose(fp);
  finish = clock();
  fprintf(stderr, "read: %.4f sec\n", (double)(finish - start) / (double)CLOCKS_PER_SEC);
  double time_startup = (double)(finish - start) / (double)CLOCKS_PER_SEC;
  log_file << "Start up took " << time_startup << " seconds." << endl;


  int count = 0;
  while(1){
    start2 = clock();
    log_file << "Ready to receive requests" << endl;
    char *buf = NULL;
    int bytes = nn_recv(socket, &buf, NN_MSG, 0);
    assert (bytes >= 0);
    P = buf;
    Psize = strlen(P);

    /* Search and print */
    size = sa_search(T, (saidx_t)n,
                     (const sauchar_t *)P, (saidx_t)Psize,
                     SA, (saidx_t)n, &left);

    const char *output;
    if(size>0){
      log_file << "yes" << endl;
      output = "1";
    } else {
      log_file << "no" << endl;
      output = "0";
    }
    int size_msg = strlen(output) + 1; // '\0' too
    bytes = nn_send(socket, output, size_msg, 0);
    assert(bytes == size_msg);

    finish2 = clock();
    double time_search = (double)(finish2 - start2) / (double)CLOCKS_PER_SEC;
    log_file << "Search number " << count << " took " << time_search << " seconds." << endl;
    count++;
  }

  /* Deallocate memory. */
  free(SA);
  free(T);

  return 0;
}
