#ifndef SMT_SEMPARSE_CONFIG_H
#define	SMT_SEMPARSE_CONFIG_H

#include <map>
#include <string>

using namespace std;

#define ARITY_SEP "@"
#define ARITY_STR "s"

namespace smt_semparse {

  /**
 * Data structure that holds a SMT semparse configuration
 */
class SMTSemparseConfig {
 public:
  SMTSemparseConfig(string settings_path, string dependencies_path, string experiment_dir, bool copy = false);
  void parse_file(string path, string* ptr_copy_location = NULL);
  string detailed_at(string key);

  map<string, string>& get_settings();
  void set_settings(string &key, string &value);

 private:
  map<string, string> settings;
};

} // namespace smt_semparse

#endif
