#include <iostream>

#include "sa_request.h"

using namespace std;

int main(int argc, char** argv) {
  SARequester requester("ipc:///tmp/sa_daemon_test.ipc");
  cout << requester.request_for_sentence("SSSSSHeidelbergEEEEE") << endl;
  cout << requester.request_for_sentence("SSSSSZentrum für Molekulare Biologie HeidelbergEEEEE") << endl;
  cout << requester.request_for_sentence("SSSSSHeidelbergasdfawerqwerfsdafgasEEEEE") << endl;
  return 0;
}
