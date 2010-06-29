// Input of the form:
// " the phantom of the opera "    tickets for <PHRASE> tonight ? ||| C=1 ||| seats for <PHRASE> ? </s> ||| C=1 ||| i see <PHRASE> ? </s> ||| C=1
//                      phrase TAB [context]+
// where    context =   phrase ||| C=...        which are separated by |||

// Model parameterised as follows:
// - each phrase, p, is allocated a latent state, t
// - this is used to generate the contexts, c
// - each context is generated using 4 independent multinomials, one for each position LL, L, R, RR

// Training with EM:
// - e-step is estimating q(t) = P(t|p,c) for all x,c
// - m-step is estimating model parameters P(c,t|p) = P(t) P(c|t)
// - PR uses alternate e-step, which first optimizes lambda 
//      min_q KL(q||p) + delta sum_pt max_c E_q[phi_ptc]
//   where
//      q(t|p,c) propto p(t,c|p) exp( -phi_ptc )
//   Then q is used to obtain expectations for vanilla M-step.

// Sexing it up:
// - learn p-specific conditionals P(t|p)
// - or generate phrase internals, e.g., generate edge words from
//   different distribution to central words
// - agreement between phrase->context model and context->phrase model

import java.io.*;
import optimization.gradientBasedMethods.*;
import optimization.gradientBasedMethods.stats.OptimizerStats;
import optimization.gradientBasedMethods.stats.ProjectedOptimizerStats;
import optimization.linesearch.ArmijoLineSearchMinimizationAlongProjectionArc;
import optimization.linesearch.GenericPickFirstStep;
import optimization.linesearch.InterpolationPickFirstStep;
import optimization.linesearch.LineSearchMethod;
import optimization.linesearch.WolfRuleLineSearch;
import optimization.projections.SimplexProjection;
import optimization.stopCriteria.CompositeStopingCriteria;
import optimization.stopCriteria.NormalizedProjectedGradientL2Norm;
import optimization.stopCriteria.NormalizedValueDifference;
import optimization.stopCriteria.ProjectedGradientL2Norm;
import optimization.stopCriteria.StopingCriteria;
import optimization.stopCriteria.ValueDifference;
import optimization.util.MathUtils;
import java.util.*;
import java.util.regex.*;
import gnu.trove.TDoubleArrayList;
import gnu.trove.TIntArrayList;
import static java.lang.Math.*;

class PhraseContextModel
{
	// model/optimisation configuration parameters
	int numTags;
	boolean posteriorRegularisation = true;
	double constraintScale = 3; // FIXME: make configurable
	
	// copied from L1LMax in depparsing code
	final double c1= 0.0001, c2=0.9, stoppingPrecision = 1e-5, maxStep = 10;
	final int maxZoomEvals = 10, maxExtrapolationIters = 200;
	int maxProjectionIterations = 200;
	int minOccurrencesForProjection = 0;

	// book keeping
	int numPositions;
	Random rng = new Random();

	// training set
	Corpus training;

	// model parameters (learnt)
	double emissions[][][]; // position in 0 .. 3 x tag x word Pr(word | tag, position)
	double prior[][]; // phrase x tag Pr(tag | phrase)
	double lambda[]; // edge = (phrase, context) x tag flattened lagrange multipliers

	PhraseContextModel(Corpus training, int tags)
	{
		this.training = training;
		this.numTags = tags;
		assert (!training.getEdges().isEmpty());
		assert (numTags > 1);

		// now initialise emissions
		numPositions = training.getEdges().get(0).getContext().size();
		assert (numPositions > 0);

		emissions = new double[numPositions][numTags][training.getNumTokens()];
		prior = new double[training.getNumEdges()][numTags];
		if (posteriorRegularisation)
			lambda = new double[training.getNumEdges() * numTags];

		for (double[][] emissionTW : emissions)
		{
			for (double[] emissionW : emissionTW)
			{
				randomise(emissionW);
//				for (int i = 0; i < emissionW.length; ++i)
//					emissionW[i] = i+1;
//				normalise(emissionW);
			}
		}
					
		for (double[] priorTag : prior)
		{
			randomise(priorTag);
//			for (int i = 0; i < priorTag.length; ++i)
//				priorTag[i] = i+1;
//			normalise(priorTag);
		}
	}

	void expectationMaximisation(int numIterations)
	{
		double lastLlh = Double.NEGATIVE_INFINITY;

		for (int iteration = 0; iteration < numIterations; ++iteration)
		{
			double emissionsCounts[][][] = new double[numPositions][numTags][training.getNumTokens()];
			double priorCounts[][] = new double[training.getNumPhrases()][numTags];
			
			// E-step
			double llh = 0;
			if (posteriorRegularisation)
			{
				EStepDualObjective objective = new EStepDualObjective();
				
				// copied from x2y2withconstraints
//				LineSearchMethod ls = new ArmijoLineSearchMinimizationAlongProjectionArc(new InterpolationPickFirstStep(1));				
//				OptimizerStats stats = new OptimizerStats();
//				ProjectedGradientDescent optimizer = new ProjectedGradientDescent(ls);
//				CompositeStopingCriteria compositeStop = new CompositeStopingCriteria();
//				compositeStop.add(new ProjectedGradientL2Norm(0.001));
//				compositeStop.add(new ValueDifference(0.001));
//				optimizer.setMaxIterations(50);
//				boolean succeed = optimizer.optimize(objective,stats,compositeStop);
				
				// copied from depparser l1lmaxobjective
				ProjectedOptimizerStats stats = new ProjectedOptimizerStats();
				GenericPickFirstStep pickFirstStep = new GenericPickFirstStep(1);
				LineSearchMethod linesearch = new WolfRuleLineSearch(pickFirstStep, c1, c2);
				ProjectedGradientDescent optimizer = new ProjectedGradientDescent(linesearch);
				optimizer.setMaxIterations(maxProjectionIterations);
		        CompositeStopingCriteria stop = new CompositeStopingCriteria();
		        stop.add(new NormalizedProjectedGradientL2Norm(stoppingPrecision));
		        stop.add(new NormalizedValueDifference(stoppingPrecision));
		        boolean succeed = optimizer.optimize(objective, stats, stop);

				System.out.println("Ended optimzation Projected Gradient Descent\n" + stats.prettyPrint(1));
				//System.out.println("Solution: " + objective.parameters);
				if (!succeed)
					System.out.println("Failed to optimize");
				//System.out.println("Ended optimization in " + optimizer.getCurrentIteration());				

				lambda = objective.getParameters();
				llh = objective.primal();
				
				for (int i = 0; i < training.getNumPhrases(); ++i)
				{
					List<Corpus.Edge> edges = training.getEdgesForPhrase(i);
					for (int j = 0; j < edges.size(); ++j)
					{
						Corpus.Edge e = edges.get(j);
						for (int t = 0; t < numTags; t++)
						{
							double p = objective.q.get(i).get(j).get(t);
							priorCounts[i][t] += e.getCount() * p;
							TIntArrayList tokens = e.getContext();
							for (int k = 0; k < tokens.size(); ++k)
								emissionsCounts[k][t][tokens.get(k)] += e.getCount() * p;
						}
					}
				}
			}
			else
			{
				for (int i = 0; i < training.getNumPhrases(); ++i)
				{
					List<Corpus.Edge> edges = training.getEdgesForPhrase(i);
					for (int j = 0; j < edges.size(); ++j)
					{
						Corpus.Edge e = edges.get(j);
						double probs[] = posterior(i, e);			
						double z = normalise(probs);
						llh += log(z) * e.getCount();
						
						TIntArrayList tokens = e.getContext();
						for (int t = 0; t < numTags; ++t)
						{
							priorCounts[i][t] += e.getCount() * probs[t];
							for (int k = 0; k < tokens.size(); ++k)
								emissionsCounts[j][t][tokens.get(k)] += e.getCount() * probs[t];
						}
					}
				}
			}

			// M-step: normalise
			for (double[][] emissionTW : emissionsCounts)
				for (double[] emissionW : emissionTW)
					normalise(emissionW);

			for (double[] priorTag : priorCounts)
				normalise(priorTag);

			emissions = emissionsCounts;
			prior = priorCounts;

			System.out.println("Iteration " + iteration + " llh " + llh);

//			if (llh - lastLlh < 1e-4)
//				break;
//			else
//				lastLlh = llh;
		}
	}

	static double normalise(double probs[])
	{
		double z = 0;
		for (double p : probs)
			z += p;
		for (int i = 0; i < probs.length; ++i)
			probs[i] /= z;
		return z;
	}

	void randomise(double probs[])
	{
		double z = 0;
		for (int i = 0; i < probs.length; ++i)
		{
			probs[i] = 10 + rng.nextDouble();
			z += probs[i];
		}

		for (int i = 0; i < probs.length; ++i)
			probs[i] /= z;
	}

	static int argmax(double probs[])
	{
		double m = Double.NEGATIVE_INFINITY;
		int mi = -1;
		for (int i = 0; i < probs.length; ++i)
		{
			if (probs[i] > m)
			{
				m = probs[i];
				mi = i;
			}
		}
		return mi;
	}

	double[] posterior(int phraseId, Corpus.Edge e) // unnormalised
	{
		double probs[] = new double[numTags];
		TIntArrayList tokens = e.getContext();
		for (int t = 0; t < numTags; ++t)
		{
			probs[t] = prior[phraseId][t];
			for (int k = 0; k < tokens.size(); ++k)
				probs[t] *= emissions[k][t][tokens.get(k)];
		}
		return probs;
	}

	void displayPosterior()
	{
		for (int i = 0; i < training.getNumPhrases(); ++i)
		{
			List<Corpus.Edge> edges = training.getEdgesForPhrase(i);
			for (Corpus.Edge e: edges)
			{
				double probs[] = posterior(i, e);
				normalise(probs);

				// emit phrase
				System.out.print(e.getPhraseString());
				System.out.print("\t");
				System.out.print(e.getContextString());
				System.out.print("||| C=" + e.getCount() + " |||");

				int t = argmax(probs);
				System.out.print(" " + t + " ||| " + probs[t]);
				// for (int t = 0; t < numTags; ++t)
				// System.out.print(" " + probs[t]);
				System.out.println();
			}
		}
	}

	public static void main(String[] args)
	{
		assert (args.length >= 2);
		try
		{
			Corpus corpus = Corpus.readFromFile(new FileReader(new File(args[0])));
			PhraseContextModel model = new PhraseContextModel(corpus, Integer.parseInt(args[1]));
			model.expectationMaximisation(Integer.parseInt(args[2]));
			model.displayPosterior();
		} 
		catch (IOException e)
		{
			System.out.println("Failed to read input file: " + args[0]);
			e.printStackTrace();
		}
	}

	class EStepDualObjective extends ProjectedObjective
	{
		List<List<TDoubleArrayList>> conditionals; // phrase id x context # x tag - precomputed
		List<List<TDoubleArrayList>> q; // ditto, but including exp(-lambda) terms
		double objective = 0; // log(z)
		// Objective.gradient = d log(z) / d lambda = E_q[phi]
		double llh = 0;

		public EStepDualObjective()
		{
			super();
			// compute conditionals p(context, tag | phrase) for all training instances
			conditionals = new ArrayList<List<TDoubleArrayList>>(training.getNumPhrases());
			q = new ArrayList<List<TDoubleArrayList>>(training.getNumPhrases());
			for (int i = 0; i < training.getNumPhrases(); ++i)
			{
				List<Corpus.Edge> edges = training.getEdgesForPhrase(i);

				conditionals.add(new ArrayList<TDoubleArrayList>(edges.size()));
				q.add(new ArrayList<TDoubleArrayList>(edges.size()));

				for (int j = 0; j < edges.size(); ++j)
				{
					Corpus.Edge e = edges.get(j);
					double probs[] = posterior(i, e);
					double z = normalise(probs);
					llh += log(z) * e.getCount();
					conditionals.get(i).add(new TDoubleArrayList(probs));
					q.get(i).add(new TDoubleArrayList(probs));
				}
			}
			
			gradient = new double[training.getNumEdges()*numTags];
			setInitialParameters(lambda);
			computeObjectiveAndGradient();
		}

		@Override
		public double[] projectPoint(double[] point)
		{
			SimplexProjection p = new SimplexProjection(constraintScale);

			double[] newPoint = point.clone();
			int edgeIndex = 0;
			for (int i = 0; i < training.getNumPhrases(); ++i)
			{
				List<Corpus.Edge> edges = training.getEdgesForPhrase(i);

				for (int t = 0; t < numTags; t++)
				{
					double[] subPoint = new double[edges.size()];
					for (int j = 0; j < edges.size(); ++j)
						subPoint[j] = point[edgeIndex+j*numTags+t];
				
					p.project(subPoint);
					for (int j = 0; j < edges.size(); ++j)
						newPoint[edgeIndex+j*numTags+t] = subPoint[j];
				}
				
				edgeIndex += edges.size() * numTags;
			}
//			System.out.println("Proj from: " + Arrays.toString(point)); 
//			System.out.println("Proj to:   " + Arrays.toString(newPoint)); 
			return newPoint;
		}

		@Override
		public void setParameters(double[] params)
		{
			super.setParameters(params);
			computeObjectiveAndGradient();
		}

		@Override
		public double[] getGradient()
		{
			gradientCalls += 1;
			return gradient;
		}

		@Override
		public double getValue()
		{
			functionCalls += 1;
			return objective;
		}

		public void computeObjectiveAndGradient()
		{
			int edgeIndex = 0;
			objective = 0;
			Arrays.fill(gradient, 0);
			for (int i = 0; i < training.getNumPhrases(); ++i)
			{
				List<Corpus.Edge> edges = training.getEdgesForPhrase(i);

				for (int j = 0; j < edges.size(); ++j)
				{
					Corpus.Edge e = edges.get(j);
					
					double z = 0;
					for (int t = 0; t < numTags; t++)
					{
						double v = conditionals.get(i).get(j).get(t) * exp(-parameters[edgeIndex+t]);
						q.get(i).get(j).set(t, v);
						z += v;
					}
					objective += log(z) * e.getCount();

					for (int t = 0; t < numTags; t++)
					{
						double v = q.get(i).get(j).get(t) / z; 
						q.get(i).get(j).set(t, v);
						gradient[edgeIndex+t] -= e.getCount() * v;
					}
					
					edgeIndex += numTags;
				}
			}			
//			System.out.println("computeObjectiveAndGradient logz=" + objective);
//			System.out.println("lambda=  " + Arrays.toString(parameters));
//			System.out.println("gradient=" + Arrays.toString(gradient));
		}

		public String toString()
		{
			StringBuilder sb = new StringBuilder();
			sb.append(getClass().getCanonicalName()).append(" with ");
			sb.append(parameters.length).append(" parameters and ");
			sb.append(training.getNumPhrases() * numTags).append(" constraints");
			return sb.toString();
		}
				
		double primal()
		{
			// primal = llh + KL(q||p) + scale * sum_pt max_c E_q[phi_pct]
			// kl = sum_Y q(Y) log q(Y) / p(Y|X)
			//    = sum_Y q(Y) { -lambda . phi(Y) - log Z }
			//    = -log Z - lambda . E_q[phi]
			//    = -objective + lambda . gradient
			
			double kl = -objective + MathUtils.dotProduct(parameters, gradient);
			double l1lmax = 0;
			for (int i = 0; i < training.getNumPhrases(); ++i)
			{
				List<Corpus.Edge> edges = training.getEdgesForPhrase(i);
				for (int t = 0; t < numTags; t++)
				{
					double lmax = Double.NEGATIVE_INFINITY;
					for (int j = 0; j < edges.size(); ++j)
						lmax = max(lmax, q.get(i).get(j).get(t));
					l1lmax += lmax;
				}
			}

			return llh + kl + constraintScale * l1lmax;
		}
	}
}
