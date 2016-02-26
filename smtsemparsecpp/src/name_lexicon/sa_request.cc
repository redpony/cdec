#include "sa_request.h"

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <nanomsg/nn.h>
#include <nanomsg/reqrep.h>
#include <iostream>
#include <stdlib.h>
#include <unistd.h>

using namespace std;

SARequester::SARequester(const char* url, int to) {
  socket = nn_socket(AF_SP, NN_REQ);
  assert(socket >= 0);
  assert(nn_connect(socket, url) >= 0);
  timeout = to;
  int return_number_rcv = nn_setsockopt(socket, NN_SOL_SOCKET, NN_RCVTIMEO, &timeout, sizeof(timeout));
  assert(return_number_rcv == 0);
}

SARequester::~SARequester() {
  nn_shutdown(socket, 0);
}

const char* SARequester::request_for_sentence(const char* sentence){
  int size_sentence = strlen(sentence) + 1;
  int bytes = nn_send(socket, sentence, size_sentence, 0);
  assert(bytes == size_sentence);
  char *buf = NULL;
  bytes = nn_recv(socket, &buf, NN_MSG, 0);
  assert(bytes >= 0);
  if(!(bytes >= 0)){
    cerr << "The suffix array daemon is not available or took longer than " << (timeout/100) << " seconds" << endl;
  }
  return buf;
}
