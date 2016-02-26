#ifndef EXTERNAL_COMMAND_CC
#define	EXTERNAL_COMMAND_CC

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <fcntl.h>
#include <string>
#include <iostream>

using namespace std;

namespace smt_semparse {

  static int exec_prog(const char **argv, string* ptr_output = NULL, string* ptr_input = NULL, string* ptr_error = NULL, int *ptr_timeout = NULL){ //give timeout a value to have a timeout
    pid_t my_pid;
    int status;
    if (0 == (my_pid = fork())) {
      if(ptr_output != NULL){
        string output = *(ptr_output);
        int fd = open(output.c_str(), O_WRONLY | O_CREAT, 0644);
        if(fd < 0){
          perror("Error");
          return -1;
        }
        if (dup2(fd, 1) == -1) {
          cerr << "could not redirect stdout to file: " << output << endl;
          return -1;
        }
        close(fd);
      }
      if(ptr_input != NULL){
        string input = *(ptr_input);
        int fd0 = open(input.c_str(), O_RDONLY);
        if(fd0 < 0){
          perror("Error");
          return -1;
        }
        if (dup2(fd0, 0) == -1) {
          cerr << "could not get stdin from file: " << input << endl;
          return -1;
        }
        close(fd0);
      }
      if(ptr_error != NULL){
        string error = *(ptr_error);
        int fd2 = open(error.c_str(), O_WRONLY | O_CREAT, 0644);
        if(fd2 < 0){
          perror("Error");
          return -1;
        }
        if (dup2(fd2, 2) == -1) {
          perror("Error");
          cerr << "could not redirect stderr to file: " << error << endl;
          return -1;
        }
        close(fd2);
      }
      if (-1 == execve(argv[0], (char **)argv , NULL)) {
        cerr << "child process execve failed" << endl;
        return -1;
      }
    }

    while (0 == waitpid(my_pid , &status , WNOHANG)) {
      if(ptr_timeout != NULL){
        if ( --*(ptr_timeout) < 0 ) {
          cerr << "the child process timed out (timeout: " << *(ptr_timeout) << ")" << endl;
          return -1;
        }
      }
      sleep(1);
    }
    if (1 != WIFEXITED(status) || 0 != WEXITSTATUS(status)) {
        cerr << "child process execve failed" << endl;
        return -1;
    }

    return 0;
  }

} // namespace smt_semparse

#endif
