#include "ff_external.h"

#include <dlfcn.h>

#include "stringlib.h"
#include "hg.h"

using namespace std;

ExternalFeature::ExternalFeature(const string& param) {
  size_t pos = param.find(' ');
  string nparam;
  string file = param;
  if (pos < param.size()) {
    nparam = Trim(param.substr(pos + 1));
    file = param.substr(0, pos);
  }
  if (file.size() < 1) {
    cerr << "External requires a path to a dynamic library!\n";
    abort();
  }
  lib_handle = dlopen(file.c_str(), RTLD_LAZY | RTLD_GLOBAL);
  if (!lib_handle) {
    cerr << "dlopen reports: " << dlerror() << endl;
    cerr << "Did you provide a full path to the dynamic library?\n";
    abort();
  }
  FeatureFunction* (*fn)(const string&) =
    (FeatureFunction* (*)(const string&))(dlsym(lib_handle, "create_ff"));
  if (!fn) {
    cerr << "dlsym reports: " << dlerror() << endl;
    abort();
  }
  ff_ext = (*fn)(nparam);
  SetStateSize(ff_ext->StateSize());
}

ExternalFeature::~ExternalFeature() {
  delete ff_ext;
  dlclose(lib_handle);
}

void ExternalFeature::PrepareForInput(const SentenceMetadata& smeta) {
  ff_ext->PrepareForInput(smeta);
}

void ExternalFeature::FinalTraversalFeatures(const void* context,
                                             SparseVector<double>* features) const {
  ff_ext->FinalTraversalFeatures(context, features);
}

void ExternalFeature::TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* context) const {
  ff_ext->TraversalFeaturesImpl(smeta, edge, ant_contexts, features, estimated_features, context);
}

