#ifndef SA_REQUEST_H
#define	SA_REQUEST_H


using namespace std;


  /**
 * Data structure that can request grammar from the extract_daemon
 */
class SARequester {
 public:
  // Sets up a Requester that will connect to the supplied url
  SARequester(const char *url, int to = 10000);

  ~SARequester();

  const char* request_for_sentence(const char *sentence);

 private:
  int socket;
  int timeout;
};

#endif
