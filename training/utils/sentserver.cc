/* Copyright (c) 2001 by David Chiang. All rights reserved.*/

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <sched.h>
#include <pthread.h>
#include <errno.h>

#include "sentserver.h"

#define MAX_CLIENTS 64

struct clientinfo {
  int s;
  struct sockaddr_in sin;
};

struct line {
  int id;
  char *s;
  int status;
  struct line *next;
} *head, **ptail;

int n_sent = 0, n_received=0, n_flushed=0;

#define STATUS_RUNNING 0
#define STATUS_ABORTED 1
#define STATUS_FINISHED 2

pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t input_mutex = PTHREAD_MUTEX_INITIALIZER;

int n_clients = 0;
int s;
int expect_multiline_output = 0;
int log_mutex = 0;
int stay_alive = 0;		/* dont panic and die with zero clients */

void queue_finish(struct line *node, char *s, int fid);
char * read_line(int fd, int multiline);
void done (int code);

struct line * queue_get(int fid) {
	struct line *cur;
	char *s, *synch;

	if (log_mutex) fprintf(stderr, "Getting for data for fid %d\n", fid);
	if (log_mutex) fprintf(stderr, "Locking queue mutex (%d)\n", fid);
	pthread_mutex_lock(&queue_mutex);

	/* First, check for aborted sentences. */

	if (log_mutex) fprintf(stderr, "  Checking queue for aborted jobs (fid %d)\n", fid);
	for (cur = head; cur != NULL; cur = cur->next) {
		if (cur->status == STATUS_ABORTED) {
			cur->status = STATUS_RUNNING;

			if (log_mutex) fprintf(stderr, "Unlocking queue mutex (%d)\n", fid);
			pthread_mutex_unlock(&queue_mutex);

			return cur;
		}
	}
	if (log_mutex) fprintf(stderr, "Unlocking queue mutex (%d)\n", fid);
	pthread_mutex_unlock(&queue_mutex);

	/* Otherwise, read a new one. */
	if (log_mutex) fprintf(stderr, "Locking input mutex (%d)\n", fid);
	if (log_mutex) fprintf(stderr, "  Reading input for new data (fid %d)\n", fid);
	pthread_mutex_lock(&input_mutex);
	s = read_line(0,0);

	while (s) {
		if (log_mutex) fprintf(stderr, "Locking queue mutex (%d)\n", fid);
		pthread_mutex_lock(&queue_mutex);
		if (log_mutex) fprintf(stderr, "Unlocking input mutex (%d)\n", fid);
		pthread_mutex_unlock(&input_mutex);

		cur = (line*)malloc(sizeof (struct line));
		cur->id = n_sent;
		cur->s = s;
		cur->next = NULL;

		*ptail = cur;
		ptail = &cur->next;

		n_sent++;

		if (strcmp(s,"===SYNCH===\n")==0){
			fprintf(stderr, "Received ===SYNCH=== signal (fid %d)\n", fid);
			// Note: queue_finish calls free(cur->s).
			// Therefore we need to create a new string here.
			synch = (char*)malloc((strlen("===SYNCH===\n")+2) * sizeof (char));
			synch = strcpy(synch, s);

			if (log_mutex) fprintf(stderr, "Unlocking queue mutex (%d)\n", fid);
			pthread_mutex_unlock(&queue_mutex);
			queue_finish(cur, synch, fid); /* handles its own lock */

			if (log_mutex) fprintf(stderr, "Locking input mutex (%d)\n", fid);
			if (log_mutex) fprintf(stderr, "  Reading input for new data (fid %d)\n", fid);
			pthread_mutex_lock(&input_mutex);

			s = read_line(0,0);
		} else {
			if (log_mutex) fprintf(stderr, "  Received new data %d (fid %d)\n", cur->id, fid);
			cur->status = STATUS_RUNNING;
			if (log_mutex) fprintf(stderr, "Unlocking queue mutex (%d)\n", fid);
			pthread_mutex_unlock(&queue_mutex);
			return cur;
		}
	}

	if (log_mutex) fprintf(stderr, "Unlocking input mutex (%d)\n", fid);
	pthread_mutex_unlock(&input_mutex);
	/* Only way to reach this point: no more output */

	if (log_mutex) fprintf(stderr, "Locking queue mutex (%d)\n", fid);
	pthread_mutex_lock(&queue_mutex);
	if (head == NULL) {
		fprintf(stderr, "Reached end of file. Exiting.\n");
		done(0);
	} else
		ptail = NULL; /* This serves as a signal that there is no more input */
	if (log_mutex) fprintf(stderr, "Unlocking queue mutex (%d)\n", fid);
	pthread_mutex_unlock(&queue_mutex);

	return NULL;
}

void queue_panic() {
	struct line *next;
	while (head && head->status == STATUS_FINISHED) {
		/* Write out finished sentences */
		if (head->status == STATUS_FINISHED) {
			fputs(head->s, stdout);
			fflush(stdout);
		}
		/* Write out blank line for unfinished sentences */
		if (head->status == STATUS_ABORTED) {
			fputs("\n", stdout);
			fflush(stdout);
		}
		/* By defition, there cannot be any RUNNING sentences, since
		function is only called when n_clients == 0 */
		free(head->s);
		next = head->next;
		free(head);
		head = next;
		n_flushed++;
	}
	fclose(stdout);
	fprintf(stderr, "All clients died. Panicking, flushing completed sentences and exiting.\n");
	done(1);
}

void queue_abort(struct line *node, int fid) {
	if (log_mutex) fprintf(stderr, "Locking queue mutex (%d)\n", fid);
	pthread_mutex_lock(&queue_mutex);
	node->status = STATUS_ABORTED;
	if (n_clients == 0) {
		if (stay_alive) {
			fprintf(stderr, "Warning! No live clients detected! Staying alive, will retry soon.\n");
		} else {
			queue_panic();
		}
	}
	if (log_mutex) fprintf(stderr, "Unlocking queue mutex (%d)\n", fid);
	pthread_mutex_unlock(&queue_mutex);
}


void queue_print() {
  struct line *cur;

  fprintf(stderr, "  Queue\n");

  for (cur = head; cur != NULL; cur = cur->next) {
    switch(cur->status) {
    case STATUS_RUNNING:
      fprintf(stderr, "    %d running  ", cur->id); break;
    case STATUS_ABORTED:
      fprintf(stderr, "    %d aborted  ", cur->id); break;
    case STATUS_FINISHED:
      fprintf(stderr, "    %d finished ", cur->id); break;

    }
	fprintf(stderr, "\n");
    //fprintf(stderr, cur->s);
  }
}

void queue_finish(struct line *node, char *s, int fid) {
  struct line *next;
  if (log_mutex) fprintf(stderr, "Locking queue mutex (%d)\n", fid);
  pthread_mutex_lock(&queue_mutex);

  free(node->s);
  node->s = s;
  node->status = STATUS_FINISHED;
  n_received++;

  /* Flush out finished nodes */
  while (head && head->status == STATUS_FINISHED) {

    if (log_mutex) fprintf(stderr, "  Flushing finished node %d\n", head->id);

    fputs(head->s, stdout);
    fflush(stdout);
    if (log_mutex) fprintf(stderr, "  Flushed node %d\n", head->id);
    free(head->s);

    next = head->next;
    free(head);

    head = next;

    n_flushed++;

    if (head == NULL) { /* empty queue */
      if (ptail == NULL) { /* This can only happen if set in queue_get as signal that there is no more input. */
        fprintf(stderr, "All sentences finished. Exiting.\n");
        done(0);
      } else /* ptail pointed at something which was just popped off the stack -- reset to head*/
        ptail = &head;
    }
  }

  if (log_mutex) fprintf(stderr, "  Flushing output %d\n", head->id);
  fflush(stdout);
  fprintf(stderr, "%d sentences sent, %d sentences finished, %d sentences flushed\n", n_sent, n_received, n_flushed);

  if (log_mutex) fprintf(stderr, "Unlocking queue mutex (%d)\n", fid);
  pthread_mutex_unlock(&queue_mutex);

}

char * read_line(int fd, int multiline) {
  int size = 80;
  char errorbuf[100];
  char *s = (char*)malloc(size+2);
  int result, errors=0;
  int i = 0;

  result = read(fd, s+i, 1);

  while (1) {
    if (result < 0) {
      perror("read()");
      sprintf(errorbuf, "Error code: %d\n", errno);
      fputs(errorbuf, stderr);
      errors++;
      if (errors > 5) {
	free(s);
	return NULL;
      } else {
	sleep(1); /* retry after delay */
      }
    } else if (result == 0) {
      break;
    } else if (multiline==0 && s[i] == '\n') {
      break;
    } else {
      if (s[i] == '\n'){
	/* if we've reached this point,
	   then multiline must be 1, and we're
	   going to poll the fd for an additional
	   line of data.  The basic design is to
	   run a select on the filedescriptor fd.
	   Select will return under two conditions:
	   if there is data on the fd, or if a
	   timeout is reached.  We'll select on this
	   fd.  If select returns because there's data
	   ready, keep going; else assume there's no
	   more and return the data we already have.
	*/

	fd_set set;
	FD_ZERO(&set);
	FD_SET(fd, &set);

	struct timeval timeout;
	timeout.tv_sec = 3; // number of seconds for timeout
	timeout.tv_usec = 0;

	int ready = select(FD_SETSIZE, &set, NULL, NULL, &timeout);
	if (ready<1){
	  break; // no more data, stop looping
	}
      }
      i++;

      if (i == size) {
	size = size*2;
	s = (char*)realloc(s, size+2);
      }
    }

    result = read(fd, s+i, 1);
  }

  if (result == 0 && i == 0) { /* end of file */
    free(s);
    return NULL;
  }

  s[i] = '\n';
  s[i+1] = '\0';

  return s;
}

void * new_client(void *arg) {
  struct clientinfo *client = (struct clientinfo *)arg;
  struct line *cur;
  int result;
  char *s;
  char errorbuf[100];

  pthread_mutex_lock(&clients_mutex);
  n_clients++;
  pthread_mutex_unlock(&clients_mutex);

  fprintf(stderr, "Client connected (%d connected)\n", n_clients);

  for (;;) {

    cur = queue_get(client->s);

    if (cur) {
      /* fprintf(stderr, "Sending to client: %s", cur->s); */
      fprintf(stderr, "Sending data %d to client (fid %d)\n", cur->id, client->s);
      result = write(client->s, cur->s, strlen(cur->s));
      if (result < strlen(cur->s)){
        perror("write()");
        sprintf(errorbuf, "Error code: %d\n", errno);
        fputs(errorbuf, stderr);

        pthread_mutex_lock(&clients_mutex);
        n_clients--;
        pthread_mutex_unlock(&clients_mutex);

        fprintf(stderr, "Client died (%d connected)\n", n_clients);
        queue_abort(cur, client->s);

        close(client->s);
        free(client);

        pthread_exit(NULL);
      }
    } else {
      close(client->s);
      pthread_mutex_lock(&clients_mutex);
      n_clients--;
      pthread_mutex_unlock(&clients_mutex);
      fprintf(stderr, "Client dismissed (%d connected)\n", n_clients);
      pthread_exit(NULL);
    }

    s = read_line(client->s,expect_multiline_output);
    if (s) {
      /* fprintf(stderr, "Client (fid %d) returned: %s", client->s, s); */
      fprintf(stderr, "Client (fid %d) returned data %d\n", client->s, cur->id);
//      queue_print();
      queue_finish(cur, s, client->s);
    } else {
      pthread_mutex_lock(&clients_mutex);
      n_clients--;
      pthread_mutex_unlock(&clients_mutex);

      fprintf(stderr, "Client died (%d connected)\n", n_clients);
      queue_abort(cur, client->s);

      close(client->s);
      free(client);

      pthread_exit(NULL);
    }

  }
  return 0;
}

void done (int code) {
  close(s);
  exit(code);
}



int main (int argc, char *argv[]) {
  struct sockaddr_in sin, from;
  int g;
  socklen_t len;
  struct clientinfo *client;
  int port;
  int opt;
  int errors = 0;
  int argi;
  char *key = NULL, *client_key;
  int use_key = 0;
  /* the key stuff here doesn't provide any
  real measure of security, it's mainly to keep
  jobs from bumping into each other.  */

  pthread_t tid;
  port = DEFAULT_PORT;

  for (argi=1; argi < argc; argi++){
    if (strcmp(argv[argi], "-m")==0){
      expect_multiline_output = 1;
    } else if (strcmp(argv[argi], "-k")==0){
      argi++;
      if (argi == argc){
      	fprintf(stderr, "Key must be specified after -k\n");
      	exit(1);
      }
      key = argv[argi];
      use_key = 1;
    } else if (strcmp(argv[argi], "--stay-alive")==0){
      stay_alive = 1;    /* dont panic and die with zero clients */
    } else {
      port = atoi(argv[argi]);
    }
  }

  /* Initialize data structures */
  head = NULL;
  ptail = &head;

  /* Set up listener */
  s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  opt = 1;
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = htonl(INADDR_ANY);
  sin.sin_port = htons(port);
  while (bind(s, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
	perror("bind()");
	sleep(1);
	errors++;
	if (errors > 100)
	  exit(1);
  }

  len = sizeof(sin);
  getsockname(s, (struct sockaddr *) &sin, &len);

  fprintf(stderr, "Listening on port %hu\n", ntohs(sin.sin_port));

  while (listen(s, MAX_CLIENTS) < 0) {
	perror("listen()");
	sleep(1);
	errors++;
	if (errors > 100)
	  exit(1);
  }

  for (;;) {
    len = sizeof(from);
    g = accept(s, (struct sockaddr *)&from, &len);
    if (g < 0) {
      perror("accept()");
      sleep(1);
      continue;
    }
    client = (clientinfo*)malloc(sizeof(struct clientinfo));
    client->s = g;
    bcopy(&from, &client->sin, len);

	if (use_key){
		fd_set set;
		FD_ZERO(&set);
		FD_SET(client->s, &set);

		struct timeval timeout;
		timeout.tv_sec = 3; // number of seconds for timeout
		timeout.tv_usec = 0;

		int ready = select(FD_SETSIZE, &set, NULL, NULL, &timeout);
		if (ready<1){
			fprintf(stderr, "Prospective client failed to respond with correct key.\n");
			close(client->s);
			free(client);
		} else {
			client_key = read_line(client->s,0);
			client_key[strlen(client_key)-1]='\0'; /* chop trailing newline */
			if (strcmp(key, client_key)==0){
				pthread_create(&tid, NULL, new_client, client);
			} else {
				fprintf(stderr, "Prospective client failed to respond with correct key.\n");
				close(client->s);
				free(client);
			}
			free(client_key);
		}
	} else {
		pthread_create(&tid, NULL, new_client, client);
	}
  }

}



