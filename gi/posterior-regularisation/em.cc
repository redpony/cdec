// Input of the form:
// " the phantom of the opera "    tickets for <PHRASE> tonight ? ||| C=1 ||| seats for <PHRASE> ? </s> ||| C=1 ||| i see <PHRASE> ? </s> ||| C=1
//                      phrase TAB [context]+
// where    context =   phrase ||| C=...        which are separated by |||

// Model parameterised as follows:
// - each phrase, p, is allocated a latent state, t
// - this is used to generate the contexts, c
// - each context is generated using 4 independent multinomials, one for each position LL, L, R, RR

// Training with EM:
// - e-step is estimating P(t|p,c) for all x,c
// - m-step is estimating model parameters P(p,c,t) = P(t) P(p|t) P(c|t)

// Sexing it up:
// - constrain the posteriors P(t|c) and P(t|p) to have few high-magnitude entries
// - improve the generation of phrase internals, e.g., generate edge words from
//   different distribution to central words

#include "alphabet.hh"
#include "log_add.hh"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <tr1/random>
#include <tr1/tuple>
#include <nlopt.h>

using namespace std;
using namespace std::tr1;

const int numTags = 5;
const int numIterations = 100;
const bool posterior_regularisation = true;
const double PHRASE_VIOLATION_WEIGHT = 10;
const double CONTEXT_VIOLATION_WEIGHT = 0;
const bool includePhraseProb = false;

// Data structures:
Alphabet<string> lexicon;
typedef vector<int> Phrase;
typedef tuple<int, int, int, int> Context;
Alphabet<Phrase> phrases;
Alphabet<Context> contexts;

typedef map<int, int> ContextCounts;
typedef map<int, int> PhraseCounts;
typedef map<int, ContextCounts> PhraseToContextCounts;
typedef map<int, PhraseCounts> ContextToPhraseCounts;

PhraseToContextCounts concordancePhraseToContexts;
ContextToPhraseCounts concordanceContextToPhrases;

typedef vector<double> Dist;
typedef vector<Dist> ConditionalDist;
Dist prior; // class -> P(class)
vector<ConditionalDist> probCtx; // word -> class -> P(word | class), for each position of context word
ConditionalDist probPhrase; // class -> P(word | class)
Dist probPhraseLength; // class -> P(length | class) expressed as geometric distribution parameter

mt19937 randomGenerator((size_t) time(NULL));
uniform_real<double> uniDist(0.0, 1e-1);
variate_generator< mt19937, uniform_real<double> > rng(randomGenerator, uniDist);

void addRandomNoise(Dist &d);
void normalise(Dist &d);
void addTo(Dist &d, const Dist &e);
int argmax(const Dist &d);

map<Phrase, map<Context, int> > lambda_indices;

Dist conditional_probs(const Phrase &phrase, const Context &context, double *normalisation = 0);
template <typename T>
Dist
penalised_conditionals(const Phrase &phrase, const Context &context, 
                       const T &lambda, double *normalisation);
//Dist penalised_conditionals(const Phrase &phrase, const Context &context, const double *lambda, double *normalisation = 0);
double penalised_log_likelihood(int n, const double *lambda, double *gradient, void *data);
void optimise_lambda(double delta, double gamma, vector<double> &lambda);
double expected_violation_phrases(const double *lambda);
double expected_violation_contexts(const double *lambda);
double primal_kl_divergence(const double *lambda);
double dual(const double *lambda);
void print_primal_dual(const double *lambda, double delta, double gamma);

ostream &operator<<(ostream &, const Phrase &);
ostream &operator<<(ostream &, const Context &);
ostream &operator<<(ostream &, const Dist &);
ostream &operator<<(ostream &, const ConditionalDist &);

int
main(int argc, char *argv[])
{
    randomGenerator.seed(time(NULL));

    int edges = 0;
    istream &input = cin;
    while (input.good())
    {
        // read the phrase
        string phraseString;
        Phrase phrase;
        getline(input, phraseString, '\t');
        istringstream pinput(phraseString);
        string token;
        while (pinput >> token)
            phrase.push_back(lexicon.insert(token));
        int phraseId = phrases.insert(phrase);

        // read the rest, storing each context
        string remainder;
        getline(input, remainder, '\n');
        istringstream rinput(remainder);
        Context context(-1, -1, -1, -1);
        int index = 0;
        while (rinput >> token)
        {
            if (token != "|||" && token != "<PHRASE>")
            {
                if (index < 4)
                {
                    // eugh! damn templates
                    switch (index)
                    {
                        case 0: get<0>(context) = lexicon.insert(token); break;
                        case 1: get<1>(context) = lexicon.insert(token); break;
                        case 2: get<2>(context) = lexicon.insert(token); break;
                        case 3: get<3>(context) = lexicon.insert(token); break;
                        default: assert(false);
                    }
                    index += 1;
                }
                else if (token.find("C=") == 0)
                {
                    int contextId = contexts.insert(context);
                    int count = atoi(token.substr(strlen("C=")).c_str());
                    concordancePhraseToContexts[phraseId][contextId] += count;
                    concordanceContextToPhrases[contextId][phraseId] += count;
                    index = 0;
                    context = Context(-1, -1, -1, -1);
                    edges += 1;
                }
            }
        }

        // trigger EOF
        input >> ws;
    }

    cout << "Read in " << phrases.size() << " phrases"
         << " and " << contexts.size() << " contexts"
         << " and " << edges << " edges"
         << " and " << lexicon.size() << " word types\n";

    // FIXME: filter out low count phrases and low count contexts (based on individual words?)
    // now populate model parameters with uniform + random noise
    prior.resize(numTags, 1.0);
    addRandomNoise(prior);
    normalise(prior);

    probCtx.resize(4, ConditionalDist(numTags, Dist(lexicon.size(), 1.0)));
    if (includePhraseProb)
        probPhrase.resize(numTags, Dist(lexicon.size(), 1.0));
    for (int t = 0; t < numTags; ++t)
    {
        for (int j = 0; j < 4; ++j)
        {
            addRandomNoise(probCtx[j][t]);
            normalise(probCtx[j][t]);
        }
        if (includePhraseProb)
        {
            addRandomNoise(probPhrase[t]);
            normalise(probPhrase[t]);
        }
    }
    if (includePhraseProb)
    {
        probPhraseLength.resize(numTags, 0.5); // geometric distribution p=0.5
        addRandomNoise(probPhraseLength);
    }

    cout << "\tprior:     " << prior << "\n";
    //cout << "\tcontext:   " << probCtx << "\n";
    //cout << "\tphrase:    " << probPhrase << "\n";
    //cout << "\tphraseLen: " << probPhraseLength << endl;

    vector<double> lambda;

    // now do EM training
    for (int iteration = 0; iteration < numIterations; ++iteration)
    {
        cout << "EM iteration " << iteration << endl;

        if (posterior_regularisation)
            optimise_lambda(PHRASE_VIOLATION_WEIGHT, CONTEXT_VIOLATION_WEIGHT, lambda);
        //cout << "\tlambda " << lambda << endl;

        Dist countsPrior(numTags, 0.0);
        vector<ConditionalDist> countsCtx(4, ConditionalDist(numTags, Dist(lexicon.size(), 1e-10)));
        ConditionalDist countsPhrase(numTags, Dist(lexicon.size(), 1e-10));
        Dist countsPhraseLength(numTags, 0.0);
        Dist nPhrases(numTags, 0.0);

        double llh = 0;
        for (PhraseToContextCounts::iterator pcit = concordancePhraseToContexts.begin();
             pcit != concordancePhraseToContexts.end(); ++pcit)
        {
            const Phrase &phrase = phrases.type(pcit->first);

            // e-step: estimate latent class probs; compile (class,word) stats for m-step
            for (ContextCounts::iterator ccit = pcit->second.begin();
                 ccit != pcit->second.end(); ++ccit)
            {
                const Context &context = contexts.type(ccit->first);

                double z = 0;
                Dist tagCounts;
                if (!posterior_regularisation)
                    tagCounts = conditional_probs(phrase, context, &z);
                else
                    tagCounts = penalised_conditionals(phrase, context, lambda, &z);

                llh += log(z) * ccit->second;
                addTo(countsPrior, tagCounts); // FIXME: times ccit->secon

                for (int t = 0; t < numTags; ++t)
                {
                    for (int j = 0; j < 4; ++j)
                        countsCtx[j][t][get<0>(context)] += tagCounts[t] * ccit->second;

                    if (includePhraseProb)
                    {
                        for (Phrase::const_iterator pit = phrase.begin(); pit != phrase.end(); ++pit)
                            countsPhrase[t][*pit] += tagCounts[t] * ccit->second;
                        countsPhraseLength[t] += phrase.size() * tagCounts[t] * ccit->second;
                        nPhrases[t] += tagCounts[t] * ccit->second;
                    }
                }
            }
        }

        cout << "M-step\n";

        // m-step: normalise prior and (class,word) stats and assign to model parameters
        normalise(countsPrior);
        prior = countsPrior;
        for (int t = 0; t < numTags; ++t)
        {
            //cout << "\t\tt " << t << " prior " << countsPrior[t] << "\n";
            for (int j = 0; j < 4; ++j)
                normalise(countsCtx[j][t]);
            if (includePhraseProb)
            {
                normalise(countsPhrase[t]);
                countsPhraseLength[t] = nPhrases[t] / countsPhraseLength[t];
            }
        }
        probCtx = countsCtx;
        if (includePhraseProb)
        {
            probPhrase = countsPhrase;
            probPhraseLength = countsPhraseLength;
        }

        double *larray = new double[lambda.size()];
        copy(lambda.begin(), lambda.end(), larray);
        print_primal_dual(larray, PHRASE_VIOLATION_WEIGHT, CONTEXT_VIOLATION_WEIGHT);
        delete [] larray;

        //cout << "\tllh " << llh << endl;
        //cout << "\tprior:     " << prior << "\n";
        //cout << "\tcontext:   " << probCtx << "\n";
        //cout << "\tphrase:    " << probPhrase << "\n";
        //cout << "\tphraseLen: " << probPhraseLength << "\n";
    }

    // output class membership
    for (PhraseToContextCounts::iterator pcit = concordancePhraseToContexts.begin();
         pcit != concordancePhraseToContexts.end(); ++pcit)
    {
        const Phrase &phrase = phrases.type(pcit->first);
        for (ContextCounts::iterator ccit = pcit->second.begin();
             ccit != pcit->second.end(); ++ccit)
        {
            const Context &context = contexts.type(ccit->first);
            Dist tagCounts = conditional_probs(phrase, context, 0);
            cout << phrase << " ||| " << context << " ||| " << argmax(tagCounts) << "\n";
        }
    }

    return 0;
}

void addRandomNoise(Dist &d)
{
    for (Dist::iterator dit = d.begin(); dit != d.end(); ++dit)
        *dit += rng();
}

void normalise(Dist &d)
{
    double z = 0;
    for (Dist::iterator dit = d.begin(); dit != d.end(); ++dit)
        z += *dit;
    for (Dist::iterator dit = d.begin(); dit != d.end(); ++dit)
        *dit /= z;
}

void addTo(Dist &d, const Dist &e)
{
    assert(d.size() == e.size());
    for (int i = 0; i < (int) d.size(); ++i)
        d[i] += e[i];
}

int argmax(const Dist &d)
{
    double best = d[0];
    int index = 0;
    for (int i = 1; i < (int) d.size(); ++i)
    {
        if (d[i] > best)
        {
            best = d[i];
            index = i;
        }
    }
    return index;
}

ostream &operator<<(ostream &out, const Phrase &phrase)
{
    for (Phrase::const_iterator pit = phrase.begin(); pit != phrase.end(); ++pit)
        lexicon.display(((pit == phrase.begin()) ? out : out << " "), *pit);
    return out;
}

ostream &operator<<(ostream &out, const Context &context)
{
    lexicon.display(out, get<0>(context));
    lexicon.display(out << " ", get<1>(context));
    lexicon.display(out << " <PHRASE> ", get<2>(context));
    lexicon.display(out << " ", get<3>(context));
    return out;
}

ostream &operator<<(ostream &out, const Dist &dist)
{
    for (Dist::const_iterator dit = dist.begin(); dit != dist.end(); ++dit)
        out << ((dit == dist.begin()) ? "" : " ") << *dit;
    return out;
}

ostream &operator<<(ostream &out, const ConditionalDist &dist)
{
    for (ConditionalDist::const_iterator dit = dist.begin(); dit != dist.end(); ++dit)
        out << ((dit == dist.begin()) ? "" : "; ") << *dit;
    return out;
}

// FIXME: slow - just use the phrase index, context index to do the mapping
// (n.b. it's a sparse setup, not just equal to 3d array index)
int
lambda_index(const Phrase &phrase, const Context &context, int tag)
{
    return lambda_indices[phrase][context] + tag;
}

template <typename T>
Dist
penalised_conditionals(const Phrase &phrase, const Context &context, 
                       const T &lambda, double *normalisation)
{
    Dist d = conditional_probs(phrase, context, 0);

    double z = 0;
    for (int t = 0; t < numTags; ++t)
    {
        d[t] *= exp(-lambda[lambda_index(phrase, context, t)]);
        z += d[t];
    }

    if (normalisation)
        *normalisation = z;

    for (int t = 0; t < numTags; ++t)
        d[t] /= z;

    return d;
}

Dist 
conditional_probs(const Phrase &phrase, const Context &context, double *normalisation)
{
    Dist tagCounts(numTags, 0.0);
    double z = 0;
    for (int t = 0; t < numTags; ++t)
    {
        double prob = prior[t];
        prob *= (probCtx[0][t][get<0>(context)] * probCtx[1][t][get<1>(context)] *
                 probCtx[2][t][get<2>(context)] * probCtx[3][t][get<3>(context)]);

        if (includePhraseProb)
        {
            prob *= pow(1 - probPhraseLength[t], phrase.size() - 1) * probPhraseLength[t];
            for (Phrase::const_iterator pit = phrase.begin(); pit != phrase.end(); ++pit)
                prob *= probPhrase[t][*pit];
        }

        tagCounts[t] = prob;
        z += prob;
    }
    if (normalisation)
        *normalisation = z;

    for (int t = 0; t < numTags; ++t)
        tagCounts[t] /= z;

    return tagCounts;
}

double 
penalised_log_likelihood(int n, const double *lambda, double *grad, void *)
{
    // return log Z(lambda, theta) over the corpus
    // where theta are the global parameters (prior, probCtx*, probPhrase*) 
    // and lambda are lagrange multipliers for the posterior sparsity constraints
    //
    // this is formulated as: 
    // f = log Z(lambda) = sum_i log ( sum_i p_theta(t_i|p_i,c_i) exp [-lambda_{t_i,p_i,c_i}] )
    // where i indexes the training examples - specifying the (p, c) pair (which may occur with count > 1)
    //
    // with derivative:
    // f'_{tpc} = frac { - count(t,p,c) p_theta(t|p,c) exp (-lambda_{t,p,c}) }
    //                 { sum_t' p_theta(t'|p,c) exp (-lambda_{t',p,c}) }

    //cout << "penalised_log_likelihood with lambda ";
    //copy(lambda, lambda+n, ostream_iterator<double>(cout, " "));
    //cout << "\n";

    double f = 0;
    if (grad)
    {
        for (int i = 0; i < n; ++i)
            grad[i] = 0.0;
    }

    for (int p = 0; p < phrases.size(); ++p)
    {
        const Phrase &phrase = phrases.type(p);
        PhraseToContextCounts::const_iterator pcit = concordancePhraseToContexts.find(p);
        for (ContextCounts::const_iterator ccit = pcit->second.begin();
             ccit != pcit->second.end(); ++ccit)
        {
            const Context &context = contexts.type(ccit->first);
            double z = 0;
            Dist scores = penalised_conditionals(phrase, context, lambda, &z);

            f += ccit->second * log(z);
            //cout << "\tphrase: " << phrase << " context: " << context << " count: " << ccit->second << " z " << z << endl;
            //cout << "\t\tscores: " << scores << "\n";

            if (grad)
            {
                for (int t = 0; t < numTags; ++t)
                {
                    int i = lambda_index(phrase, context, t); // FIXME: redundant lookups
                    assert(grad[i] == 0.0);
                    grad[i] = - ccit->second * scores[t];
                }
            }
        }
    }

    //cout << "penalised_log_likelihood returning " << f;
    //if (grad)
    //{
        //cout << "\ngradient: ";
        //copy(grad, grad+n, ostream_iterator<double>(cout, " "));
    //}
    //cout << "\n";

    return f;
}

typedef struct 
{
    // one of p or c should be set to -1, in which case it will be marginalised out 
    // i.e. sum_p' lambda_{p'ct} <= threshold
    //   or sum_c' lambda_{pc't} <= threshold
    int p, c, t, threshold;
} constraint_data;

double 
constraint_and_gradient(int n, const double *lambda, double *grad, void *data)
{
    constraint_data *d = (constraint_data *) data;
    assert(d->t >= 0);
    assert(d->threshold >= 0);

    //cout << "constraint_and_gradient: t " << d->t << " p " << d->p << " c " << d->c << " tau " << d->threshold << endl;
    //cout << "\tlambda ";
    //copy(lambda, lambda+n, ostream_iterator<double>(cout, " "));
    //cout << "\n";

    // FIXME: it's crazy to use a dense gradient here => will only have a handful of non-zero entries
    if (grad)
    {
        for (int i = 0; i < n; ++i)
            grad[i] = 0.0;
    }

    //cout << "constraint_and_gradient: " << d->p << "; " << d->c << "; " << d->t << "; " << d->threshold << endl;

    if (d->p >= 0)
    {
        assert(d->c < 0);
        //    sum_c lambda_pct          <= delta [a.k.a. threshold]
        // => sum_c lambda_pct - delta  <= 0
        // derivative_pct = { 1, if p and t match; 0, otherwise }

        double val = -d->threshold;

        const Phrase &phrase = phrases.type(d->p);
        PhraseToContextCounts::const_iterator pcit = concordancePhraseToContexts.find(d->p);
        assert(pcit != concordancePhraseToContexts.end());
        for (ContextCounts::const_iterator ccit = pcit->second.begin();
             ccit != pcit->second.end(); ++ccit)
        {
            const Context &context = contexts.type(ccit->first);
            int i = lambda_index(phrase, context, d->t);
            val += lambda[i];
            if (grad) grad[i] = 1;
        }
        //cout << "\treturning " << val << endl;

        return val;
    }
    else
    {
        assert(d->c >= 0);
        assert(d->p < 0);
        //    sum_p lambda_pct          <= gamma [a.k.a. threshold]
        // => sum_p lambda_pct - gamma  <= 0
        // derivative_pct = { 1, if c and t match; 0, otherwise }

        double val = -d->threshold;

        const Context &context = contexts.type(d->c);
        ContextToPhraseCounts::iterator cpit = concordanceContextToPhrases.find(d->c);
        assert(cpit != concordanceContextToPhrases.end());
        for (PhraseCounts::iterator pcit = cpit->second.begin();
             pcit != cpit->second.end(); ++pcit)
        {
            const Phrase &phrase = phrases.type(pcit->first);
            int i = lambda_index(phrase, context, d->t);
            val += lambda[i];
            if (grad) grad[i] = 1;
        }
        //cout << "\treturning " << val << endl;

        return val;
    }
}

void
optimise_lambda(double delta, double gamma, vector<double> &lambdav)
{
    int num_lambdas = lambdav.size();
    if (lambda_indices.empty() || lambdav.empty())
    {
        lambda_indices.clear();
        lambdav.clear();

        int i = 0;
        for (int p = 0; p < phrases.size(); ++p)
        {
            const Phrase &phrase = phrases.type(p);
            PhraseToContextCounts::iterator pcit = concordancePhraseToContexts.find(p);
            for (ContextCounts::iterator ccit = pcit->second.begin();
                 ccit != pcit->second.end(); ++ccit)
            {
                const Context &context = contexts.type(ccit->first);
                lambda_indices[phrase][context] = i;
                i += numTags;
            }
        }
        num_lambdas = i;
        lambdav.resize(num_lambdas);
    }
    //cout << "optimise_lambda: #langrange multipliers " << num_lambdas << endl;

    // FIXME: better to work with an implicit representation to save memory usage
    int num_constraints = (((delta > 0) ? phrases.size() : 0) + ((gamma > 0) ? contexts.size() : 0)) * numTags;
    //cout << "optimise_lambda: #constraints " << num_constraints << endl;
    constraint_data *data = new constraint_data[num_constraints];
    int i = 0;
    if (delta > 0)
    {
        for (int p = 0; p < phrases.size(); ++p)
        {
            for (int t = 0; t < numTags; ++t, ++i)
            {
                constraint_data &d = data[i];
                d.p = p;
                d.c = -1;
                d.t = t;
                d.threshold = delta;
            }
        }
    }

    if (gamma > 0)
    {
        for (int c = 0; c < contexts.size(); ++c)
        {
            for (int t = 0; t < numTags; ++t, ++i)
            {
                constraint_data &d = data[i];
                d.p = -1;
                d.c = c;
                d.t = t;
                d.threshold = gamma;
            }
        }
    }
    assert(i == num_constraints);

    double lambda[num_lambdas];
    double lb[num_lambdas], ub[num_lambdas];
    for (i = 0; i < num_lambdas; ++i)
    {
        lambda[i] = lambdav[i]; // starting value
        lb[i] = 0;              // lower bound
        if (delta <= 0)         // upper bound
            ub[i] = gamma;      
        else if (gamma <= 0)
            ub[i] = delta;
        else
            assert(false);
    }

    //print_primal_dual(lambda, delta, gamma);
   
    double minf;
    int error_code = nlopt_minimize_constrained(NLOPT_LN_COBYLA, num_lambdas, penalised_log_likelihood, NULL,
                                                num_constraints, constraint_and_gradient, data, sizeof(constraint_data),
                                                lb, ub, lambda, &minf, -HUGE_VAL, 0.0, 0.0, 1e-4, NULL, 0, 0.0);
    //cout << "optimise error code " << error_code << endl;

    //print_primal_dual(lambda, delta, gamma);

    delete [] data;

    if (error_code < 0)
        cout << "WARNING: optimisation failed with error code: " << error_code << endl;
    //else
    //{
        //cout << "success; minf " << minf << endl;
        //print_primal_dual(lambda, delta, gamma);
    //}

    lambdav = vector<double>(&lambda[0], &lambda[0] + num_lambdas);
}

// FIXME: inefficient - cache the scores
double
expected_violation_phrases(const double *lambda)
{
    // sum_pt max_c E_q[phi_pct]
    double violation = 0;

    for (int p = 0; p < phrases.size(); ++p)
    {
        const Phrase &phrase = phrases.type(p);
        PhraseToContextCounts::const_iterator pcit = concordancePhraseToContexts.find(p);

        for (int t = 0; t < numTags; ++t)
        {
            double best = 0;
            for (ContextCounts::const_iterator ccit = pcit->second.begin();
                 ccit != pcit->second.end(); ++ccit)
            {
                const Context &context = contexts.type(ccit->first);
                Dist scores = penalised_conditionals(phrase, context, lambda, 0);
                best = max(best, scores[t]);
            }
            violation += best;
        }
    }

    return violation;
}

// FIXME: inefficient - cache the scores
double
expected_violation_contexts(const double *lambda)
{
    // sum_ct max_p E_q[phi_pct]
    double violation = 0;

    for (int c = 0; c < contexts.size(); ++c)
    {
        const Context &context = contexts.type(c);
        ContextToPhraseCounts::iterator cpit = concordanceContextToPhrases.find(c);

        for (int t = 0; t < numTags; ++t)
        {
            double best = 0;
            for (PhraseCounts::iterator pit = cpit->second.begin();
                 pit != cpit->second.end(); ++pit)
            {
                const Phrase &phrase = phrases.type(pit->first);
                Dist scores = penalised_conditionals(phrase, context, lambda, 0);
                best = max(best, scores[t]);
            }
            violation += best;
        }
    }

    return violation;
}

// FIXME: possibly inefficient
double 
primal_likelihood() // FIXME: primal evaluation needs to use lambda and calculate l1linf terms
{
    double llh = 0;
    for (int p = 0; p < phrases.size(); ++p)
    {
        const Phrase &phrase = phrases.type(p);
        PhraseToContextCounts::const_iterator pcit = concordancePhraseToContexts.find(p);
        for (ContextCounts::const_iterator ccit = pcit->second.begin();
             ccit != pcit->second.end(); ++ccit)
        {
            const Context &context = contexts.type(ccit->first);
            double z = 0;
            Dist scores = conditional_probs(phrase, context, &z);
            llh += ccit->second * log(z);
        }
    }
    return llh;
}

// FIXME: inefficient - cache the scores
double 
primal_kl_divergence(const double *lambda)
{
    // return KL(q || p) = sum_y q(y) { log q(y) - log p(y | x) }
    //                   = sum_y q(y) { log p(y | x) - lambda . phi(x, y) - log Z - log p(y | x) }
    //                   = sum_y q(y) { - lambda . phi(x, y) } - log Z
    // and q(y) factors with each edge, ditto for Z
    
    double feature_sum = 0, log_z = 0;
    for (int p = 0; p < phrases.size(); ++p)
    {
        const Phrase &phrase = phrases.type(p);
        PhraseToContextCounts::const_iterator pcit = concordancePhraseToContexts.find(p);
        for (ContextCounts::const_iterator ccit = pcit->second.begin();
             ccit != pcit->second.end(); ++ccit)
        {
            const Context &context = contexts.type(ccit->first);

            double local_z = 0;
            double local_f = 0;
            Dist d = conditional_probs(phrase, context, 0);
            for (int t = 0; t < numTags; ++t)
            {
                int i = lambda_index(phrase, context, t);
                double s = d[t] * exp(-lambda[i]);
                local_f += lambda[i] * s;
                local_z += s;
            }

            log_z += ccit->second * log(local_z);
            feature_sum += ccit->second * (local_f / local_z);
        }
    }

    return -feature_sum - log_z;
}

// FIXME: inefficient - cache the scores
double 
dual(const double *lambda)
{
    // return log(Z) = - log { sum_y p(y | x) exp( - lambda . phi(x, y) }
    // n.b. have flipped the sign as we're minimising
    
    double z = 0;
    for (int p = 0; p < phrases.size(); ++p)
    {
        const Phrase &phrase = phrases.type(p);
        PhraseToContextCounts::const_iterator pcit = concordancePhraseToContexts.find(p);
        for (ContextCounts::const_iterator ccit = pcit->second.begin();
             ccit != pcit->second.end(); ++ccit)
        {
            const Context &context = contexts.type(ccit->first);
            double lz = 0;
            Dist scores = penalised_conditionals(phrase, context, lambda, &z);
            z += lz * ccit->second;
        }
    }
    return log(z);
}

void
print_primal_dual(const double *lambda, double delta, double gamma)
{
    double likelihood = primal_likelihood();
    double kl = primal_kl_divergence(lambda);
    double sum_pt = expected_violation_phrases(lambda);
    double sum_ct = expected_violation_contexts(lambda);
    //double d = dual(lambda);

    cout << "\tllh=" << likelihood
         << " kl=" << kl
         << " violations phrases=" << sum_pt
         << " contexts=" << sum_ct
         //<< " primal=" << (kl + delta * sum_pt + gamma * sum_ct) 
         //<< " dual=" << d
         << " objective=" << (likelihood - kl + delta * sum_pt + gamma * sum_ct) 
         << endl;
}
