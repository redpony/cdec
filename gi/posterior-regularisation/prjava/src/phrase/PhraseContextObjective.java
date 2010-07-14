package phrase;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Future;

import optimization.gradientBasedMethods.ProjectedGradientDescent;
import optimization.gradientBasedMethods.ProjectedObjective;
import optimization.gradientBasedMethods.stats.OptimizerStats;
import optimization.linesearch.ArmijoLineSearchMinimizationAlongProjectionArc;
import optimization.linesearch.InterpolationPickFirstStep;
import optimization.linesearch.LineSearchMethod;
import optimization.projections.SimplexProjection;
import optimization.stopCriteria.CompositeStopingCriteria;
import optimization.stopCriteria.ProjectedGradientL2Norm;
import optimization.stopCriteria.StopingCriteria;
import optimization.stopCriteria.ValueDifference;
import optimization.util.MathUtils;
import phrase.Corpus.Edge;

public class PhraseContextObjective extends ProjectedObjective
{
	private static final double GRAD_DIFF = 0.00002;
	private static double INIT_STEP_SIZE = 300;
	private static double VAL_DIFF = 1e-4; // FIXME needs to be tuned
	private static int ITERATIONS = 100;
	
	private PhraseCluster c;
	
	// un-regularized unnormalized posterior, p[edge][tag]
	// P(tag|edge) \propto P(tag|phrase)P(context|tag)
	private double p[][];

	// regularized unnormalized posterior 
	// q[edge][tag] propto p[edge][tag]*exp(-lambda)
	private double q[][];
	private List<Corpus.Edge> data;
	
	// log likelihood under q
	private double loglikelihood;
	private SimplexProjection projectionPhrase;
	private SimplexProjection projectionContext;
	
	double[] newPoint;
	private int n_param;
	
	// likelihood under p
	public double llh;
	
	private Map<Corpus.Edge, Integer> edgeIndex;
	
	private long projectionTime;
	private long objectiveTime;
	private long actualProjectionTime;
	private ExecutorService pool;
	
	double scalePT;
	double scaleCT;
	
	public PhraseContextObjective(PhraseCluster cluster, double[] startingParameters, ExecutorService pool,
			double scalePT, double scaleCT)
	{
		c=cluster;
		data=c.c.getEdges();
		n_param=data.size()*c.K*2;
		this.pool=pool;
		this.scalePT = scalePT;
		this.scaleCT = scaleCT;
		
		parameters = startingParameters;
		if (parameters == null)
			parameters = new double[n_param];
		
		System.out.println("Num parameters " + n_param);
		newPoint = new double[n_param];
		gradient = new double[n_param];
		initP();
		projectionPhrase = new SimplexProjection(scalePT);
		projectionContext = new SimplexProjection(scaleCT);
		q=new double [data.size()][c.K];
		
		edgeIndex = new HashMap<Edge, Integer>();
		for (int e=0; e<data.size(); e++)
			edgeIndex.put(data.get(e), e);

		setParameters(parameters);
	}

	private void initP(){
		p=new double[data.size()][];
		for(int edge=0;edge<data.size();edge++)
		{
			p[edge]=c.posterior(data.get(edge));
			llh += data.get(edge).getCount() * Math.log(arr.F.l1norm(p[edge]));
			arr.F.l1normalize(p[edge]);
		}
	}
	
	@Override
	public void setParameters(double[] params) {
		//System.out.println("setParameters " + Arrays.toString(parameters));
		// TODO: test if params have changed and skip update otherwise
		super.setParameters(params);
		updateFunction();
	}
	
	private void updateFunction()
	{
		updateCalls++;
		loglikelihood=0;
		System.out.print(".");
		System.out.flush();

		long begin = System.currentTimeMillis();
		for (int e=0; e<data.size(); e++) 
		{
			Edge edge = data.get(e);
			int offset = edgeIndex.get(edge)*c.K*2;
			for(int tag=0; tag<c.K; tag++)
			{
				int ip = offset + tag*2;
				int ic = ip + 1;
				q[e][tag] = p[e][tag]*
					Math.exp((-parameters[ip]-parameters[ic]) / edge.getCount());
			}
		}
	
		for(int edge=0;edge<data.size();edge++){
			loglikelihood+=data.get(edge).getCount() * Math.log(arr.F.l1norm(q[edge]));
			arr.F.l1normalize(q[edge]);
		}
		
		for (int e=0; e<data.size(); e++) 
		{
			Edge edge = data.get(e);
			int offset = edgeIndex.get(edge)*c.K*2;
			for(int tag=0; tag<c.K; tag++)
			{
				int ip = offset + tag*2;
				int ic = ip + 1;		
				gradient[ip]=-q[e][tag];
				gradient[ic]=-q[e][tag];
			}
		}
		//System.out.println("objective " + loglikelihood + " ||gradient||_2: " + arr.F.l2norm(gradient));		
		objectiveTime += System.currentTimeMillis() - begin;
	}
	
	@Override
	public double[] projectPoint(double[] point) 
	{
		long begin = System.currentTimeMillis();
		List<Future<?>> tasks = new ArrayList<Future<?>>();
		
		System.out.print(",");
		System.out.flush();

		//System.out.println("\t\tprojectPoint: " + Arrays.toString(point));
		Arrays.fill(newPoint, 0, newPoint.length, 0);
		
		// first project using the phrase-tag constraints,
		// for all p,t: sum_c lambda_ptc < scaleP 
		if (pool == null)
		{
			for (int p = 0; p < c.c.getNumPhrases(); ++p)
			{
				List<Edge> edges = c.c.getEdgesForPhrase(p);
				double[] toProject = new double[edges.size()];
				for(int tag=0;tag<c.K;tag++)
				{
					for(int e=0; e<edges.size(); e++)
						toProject[e] = point[index(edges.get(e), tag, true)];
					long lbegin = System.currentTimeMillis();
					projectionPhrase.project(toProject);
					actualProjectionTime += System.currentTimeMillis() - lbegin;
					for(int e=0; e<edges.size(); e++)
						newPoint[index(edges.get(e), tag, true)] = toProject[e];
				}
			}
		}
		else // do above in parallel using thread pool
		{	
			for (int p = 0; p < c.c.getNumPhrases(); ++p)
			{
				final int phrase = p;
				final double[] inPoint = point;
				Runnable task = new Runnable()
				{
					public void run()
					{
						List<Edge> edges = c.c.getEdgesForPhrase(phrase);
						double toProject[] = new double[edges.size()];
						for(int tag=0;tag<c.K;tag++)
						{
							for(int e=0; e<edges.size(); e++)
								toProject[e] = inPoint[index(edges.get(e), tag, true)];
							projectionPhrase.project(toProject);
							for(int e=0; e<edges.size(); e++)
								newPoint[index(edges.get(e), tag, true)] = toProject[e];
						}
					}		
				};
				tasks.add(pool.submit(task));
			}
		}
		//System.out.println("after PT " + Arrays.toString(newPoint));
	
		// now project using the context-tag constraints,
		// for all c,t: sum_p omega_pct < scaleC
		if (pool == null)
		{
			for (int ctx = 0; ctx < c.c.getNumContexts(); ++ctx)
			{
				List<Edge> edges = c.c.getEdgesForContext(ctx);
				double toProject[] = new double[edges.size()];
				for(int tag=0;tag<c.K;tag++)
				{
					for(int e=0; e<edges.size(); e++)
						toProject[e] = point[index(edges.get(e), tag, false)];
					long lbegin = System.currentTimeMillis();
					projectionContext.project(toProject);
					actualProjectionTime += System.currentTimeMillis() - lbegin;
					for(int e=0; e<edges.size(); e++)
						newPoint[index(edges.get(e), tag, false)] = toProject[e];
				}
			}
		}
		else
		{
			// do above in parallel using thread pool
			for (int ctx = 0; ctx < c.c.getNumContexts(); ++ctx)
			{
				final int context = ctx;
				final double[] inPoint = point;
				Runnable task = new Runnable()
				{
					public void run()
					{
						List<Edge> edges = c.c.getEdgesForContext(context);
						double toProject[] = new double[edges.size()];
						for(int tag=0;tag<c.K;tag++)
						{
							for(int e=0; e<edges.size(); e++)
								toProject[e] = inPoint[index(edges.get(e), tag, false)];
							projectionContext.project(toProject);
							for(int e=0; e<edges.size(); e++)
								newPoint[index(edges.get(e), tag, false)] = toProject[e];
						}
					}
				};
				tasks.add(pool.submit(task));
			}
		}
		
		if (pool != null)
		{
			// wait for all the jobs to complete
			Exception failure = null;
			for (Future<?> task: tasks)
			{
				try {
					task.get();
				} catch (InterruptedException e) {
					System.err.println("ERROR: Projection thread interrupted");
					e.printStackTrace();
					failure = e;
				} catch (ExecutionException e) {
					System.err.println("ERROR: Projection thread died");
					e.printStackTrace();
					failure = e;
				}
			}
			// rethrow the exception
			if (failure != null)
				throw new RuntimeException(failure);
		}
		
		double[] tmp = newPoint;
		newPoint = point;
		projectionTime += System.currentTimeMillis() - begin;
		
		//System.out.println("\t\treturning " + Arrays.toString(tmp));
		return tmp;
	}
	
	private int index(Edge edge, int tag, boolean phrase)
	{
		// NB if indexing changes must also change code in updateFunction and constructor
		if (phrase)
			return edgeIndex.get(edge)*c.K*2 + tag*2;
		else
			return edgeIndex.get(edge)*c.K*2 + tag*2 + 1;
	}

	@Override
	public double[] getGradient() {
		gradientCalls++;
		return gradient;
	}

	@Override
	public double getValue() {
		functionCalls++;
		return loglikelihood;
	}

	@Override
	public String toString() {
		return "No need for pointless toString";
	}

	public double []posterior(int edgeIndex){
		return q[edgeIndex];
	}
	
	public boolean optimizeWithProjectedGradientDescent()
	{
		projectionTime = 0;
		actualProjectionTime = 0;
		objectiveTime = 0;
		long start = System.currentTimeMillis();

		LineSearchMethod ls =
			new ArmijoLineSearchMinimizationAlongProjectionArc
				(new InterpolationPickFirstStep(INIT_STEP_SIZE));
		//LineSearchMethod  ls = new WolfRuleLineSearch(
		//		(new InterpolationPickFirstStep(INIT_STEP_SIZE)), c1, c2);
		OptimizerStats stats = new OptimizerStats();
		
		
		ProjectedGradientDescent optimizer = new ProjectedGradientDescent(ls);
		StopingCriteria stopGrad = new ProjectedGradientL2Norm(GRAD_DIFF);
		StopingCriteria stopValue = new ValueDifference(VAL_DIFF*(-llh));
		CompositeStopingCriteria compositeStop = new CompositeStopingCriteria();
		compositeStop.add(stopGrad);
		compositeStop.add(stopValue);
		optimizer.setMaxIterations(ITERATIONS);
		updateFunction();
		boolean success = optimizer.optimize(this,stats,compositeStop);
//		System.out.println("Ended optimzation Projected Gradient Descent\n" + stats.prettyPrint(1));

		System.out.println();
		
		if (success)
			System.out.print("\toptimization took " + optimizer.getCurrentIteration() + " iterations");
	 	else
			System.out.print("\toptimization failed to converge");
		long total = System.currentTimeMillis() - start;
		System.out.println(" and " + total + " ms: projection " + projectionTime + 
				" actual " + actualProjectionTime + " objective " + objectiveTime);

		return success;
	}
	
	double loglikelihood()
	{
		return llh;
	}
	
	double KL_divergence()
	{
		return -loglikelihood + MathUtils.dotProduct(parameters, gradient);
	}
	
	double phrase_l1lmax()
	{
		// \sum_{tag,phrase} max_{context} P(tag|context,phrase)
		double sum=0;
		for (int p = 0; p < c.c.getNumPhrases(); ++p)
		{
			List<Edge> edges = c.c.getEdgesForPhrase(p);
			for(int tag=0;tag<c.K;tag++)
			{
				double max=0;
				for (Edge edge: edges)
					max = Math.max(max, q[edgeIndex.get(edge)][tag]);
				sum+=max;
			}	
		}
		return sum;
	}
	
	double context_l1lmax()
	{
		// \sum_{tag,context} max_{phrase} P(tag|context,phrase)
		double sum=0;
		for (int ctx = 0; ctx < c.c.getNumContexts(); ++ctx)
		{
			List<Edge> edges = c.c.getEdgesForContext(ctx);
			for(int tag=0; tag<c.K; tag++)
			{
				double max=0;
				for (Edge edge: edges)
					max = Math.max(max, q[edgeIndex.get(edge)][tag]);
				sum+=max;
			}	
		}
		return sum;
	}
	
	// L - KL(q||p) - scalePT * l1lmax_phrase - scaleCT * l1lmax_context
	public double primal()
	{
		return loglikelihood() - KL_divergence() - scalePT * phrase_l1lmax() - scaleCT * context_l1lmax();
	}
}