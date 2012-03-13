#include "pyp_word_model.h"

#include <iostream>

using namespace std;

void PYPWordModel::ResampleHyperparameters(MT19937* rng) {
  r.resample_hyperparameters(rng);
  cerr << " PYPWordModel(d=" << r.discount() << ",s=" << r.strength() << ")\n";
}

void PYPWordModel::Summary() const {
  cerr << "PYPWordModel: generations=" << r.num_customers()
       << " PYP(d=" << r.discount() << ",s=" << r.strength() << ')' << endl;
  for (CCRP<vector<WordID> >::const_iterator it = r.begin(); it != r.end(); ++it)
    cerr << "   " << it->second.total_dish_count_
              << " (on " << it->second.table_counts_.size() << " tables) "
              << TD::GetString(it->first) << endl;
}

