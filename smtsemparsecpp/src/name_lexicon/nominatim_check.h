#ifndef NOMINATIM_CHECK_H
#define	NOMINATIM_CHECK_H

#include "../name_lexicon/sa_request.h"


using namespace std;


  /**
 * Data structure that can request grammar from the extract_daemon
 */
class NominatimCheck {
 public:
  // Sets up a Requester that will connect to the supplied url
  NominatimCheck(const char *url, int to = 10000);

  int protect_sentence_for_nominatim(string* ptr_sentence, string* ptr_parallel_sentence = NULL);

 private:
  SARequester requester;
  int timeout;
};

#endif
