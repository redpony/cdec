#include <iostream>
#include <sstream>
#include <vector>

#include <map>
#include <boost/program_options.hpp>
#include <boost/shared_ptr.hpp>

#include "filelib.h"
#include "aligner.h"

namespace po = boost::program_options;
using namespace std;
using boost::shared_ptr;

struct Command {
  virtual ~Command() {}
  virtual string Name() const = 0;

  // returns 1 for alignment grid output [default]
  // returns 2 if Summary() should be called [for AER, etc]
  virtual int Result() const { return 1; }

  virtual bool RequiresTwoOperands() const { return true; }
  virtual void Apply(const Array2D<bool>& a, const Array2D<bool>& b, Array2D<bool>* x) = 0;
  void EnsureSize(const Array2D<bool>& a, const Array2D<bool>& b, Array2D<bool>* x) {
    x->resize(max(a.width(), b.width()), max(a.height(), b.width()));
  }
  bool Safe(const Array2D<bool>& a, int i, int j) const {
    if (i < a.width() && j < a.height())
      return a(i,j);
    else
      return false;
  }
  virtual void Summary() { assert(!"Summary should have been overridden"); }
};

// compute fmeasure, second alignment is reference, first is hyp
struct FMeasureCommand : public Command {
  FMeasureCommand() : matches(), num_predicted(), num_in_ref() {}
  int Result() const { return 2; }
  string Name() const { return "f"; }
  bool RequiresTwoOperands() const { return true; }
  void Apply(const Array2D<bool>& hyp, const Array2D<bool>& ref, Array2D<bool>* x) {
    int i_len = ref.width();
    int j_len = ref.height();
    for (int i = 0; i < i_len; ++i) {
      for (int j = 0; j < j_len; ++j) {
        if (ref(i,j)) {
          ++num_in_ref;
          if (Safe(hyp, i, j)) ++matches;
        } 
      }
    }
    for (int i = 0; i < hyp.width(); ++i)
      for (int j = 0; j < hyp.height(); ++j)
        if (hyp(i,j)) ++num_predicted;
  }
  void Summary() {
    if (num_predicted == 0 || num_in_ref == 0) {
      cerr << "Insufficient statistics to compute f-measure!\n";
      abort();
    }
    const double prec = static_cast<double>(matches) / num_predicted;
    const double rec = static_cast<double>(matches) / num_in_ref;
    cout << "P: " << prec << endl;
    cout << "R: " << rec << endl;
    const double f = (2.0 * prec * rec) / (rec + prec);
    cout << "F: " << f << endl;
  }
  int matches;
  int num_predicted;
  int num_in_ref;
};

struct ConvertCommand : public Command {
  string Name() const { return "convert"; }
  bool RequiresTwoOperands() const { return false; }
  void Apply(const Array2D<bool>& in, const Array2D<bool>&not_used, Array2D<bool>* x) {
    *x = in;
  }
};

struct InvertCommand : public Command {
  string Name() const { return "invert"; }
  bool RequiresTwoOperands() const { return false; }
  void Apply(const Array2D<bool>& in, const Array2D<bool>&not_used, Array2D<bool>* x) {
    Array2D<bool>& res = *x;
    res.resize(in.height(), in.width());
    for (int i = 0; i < in.height(); ++i)
      for (int j = 0; j < in.width(); ++j)
        res(i, j) = in(j, i);
  }
};

struct IntersectCommand : public Command {
  string Name() const { return "intersect"; }
  bool RequiresTwoOperands() const { return true; }
  void Apply(const Array2D<bool>& a, const Array2D<bool>& b, Array2D<bool>* x) {
    EnsureSize(a, b, x);
    Array2D<bool>& res = *x;
    for (int i = 0; i < a.width(); ++i)
      for (int j = 0; j < a.height(); ++j)
        res(i, j) = Safe(a, i, j) && Safe(b, i, j);
  }
};

map<string, boost::shared_ptr<Command> > commands;

void InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  ostringstream os;
  os << "[REQ] Operation to perform:";
  for (map<string, boost::shared_ptr<Command> >::iterator it = commands.begin();
       it != commands.end(); ++it) {
    os << ' ' << it->first;
  }
  string cstr = os.str();
  opts.add_options()
        ("input_1,i", po::value<string>(), "[REQ] Alignment 1 file, - for STDIN")
        ("input_2,j", po::value<string>(), "[OPT] Alignment 2 file, - for STDIN")
	("command,c", po::value<string>()->default_value("convert"), cstr.c_str())
        ("help,h", "Print this help message and exit");
  po::options_description clo("Command line options");
  po::options_description dcmdline_options;
  dcmdline_options.add(opts);

  po::store(parse_command_line(argc, argv, dcmdline_options), *conf);
  po::notify(*conf);

  if (conf->count("help") || conf->count("input_1") == 0 || conf->count("command") == 0) {
    cerr << dcmdline_options << endl;
    exit(1);
  }
  const string cmd = (*conf)["command"].as<string>();
  if (commands.count(cmd) == 0) {
    cerr << "Don't understand command: " << cmd << endl;
    exit(1);
  }
  if (commands[cmd]->RequiresTwoOperands()) {
    if (conf->count("input_2") == 0) {
      cerr << "Command '" << cmd << "' requires two alignment files\n";
      exit(1);
    }
    if ((*conf)["input_1"].as<string>() == "-" && (*conf)["input_2"].as<string>() == "-") {
      cerr << "Both inputs cannot be STDIN\n";
      exit(1);
    }
  } else {
    if (conf->count("input_2") != 0) {
      cerr << "Command '" << cmd << "' requires only one alignment file\n";
      exit(1);
    }
  }
}

template<class C> static void AddCommand() {
  C* c = new C;
  commands[c->Name()].reset(c);
}

int main(int argc, char **argv) {
  AddCommand<ConvertCommand>();
  AddCommand<InvertCommand>();
  AddCommand<IntersectCommand>();
  AddCommand<FMeasureCommand>();
  po::variables_map conf;
  InitCommandLine(argc, argv, &conf);
  Command& cmd = *commands[conf["command"].as<string>()];
  boost::shared_ptr<ReadFile> rf1(new ReadFile(conf["input_1"].as<string>()));
  boost::shared_ptr<ReadFile> rf2;
  if (cmd.RequiresTwoOperands())
    rf2.reset(new ReadFile(conf["input_2"].as<string>()));
  istream* in1 = rf1->stream();
  istream* in2 = NULL;
  if (rf2) in2 = rf2->stream();
  while(*in1) {
    string line1;
    string line2;
    getline(*in1, line1);
    if (in2) {
      getline(*in2, line2);
      if ((*in1 && !*in2) || (*in2 && !*in1)) {
        cerr << "Mismatched number of lines!\n";
        exit(1);
      }
    }
    if (line1.empty() && !*in1) break;
    shared_ptr<Array2D<bool> > out(new Array2D<bool>);
    shared_ptr<Array2D<bool> > a1 = AlignerTools::ReadPharaohAlignmentGrid(line1);
    if (in2) {
      shared_ptr<Array2D<bool> > a2 = AlignerTools::ReadPharaohAlignmentGrid(line2);
      cmd.Apply(*a1, *a2, out.get());
    } else {
      Array2D<bool> dummy;
      cmd.Apply(*a1, dummy, out.get());
    }
    
    if (cmd.Result() == 1) {
      AlignerTools::SerializePharaohFormat(*out, &cout);
    }
  }
  if (cmd.Result() == 2)
    cmd.Summary();
  return 0;
}

