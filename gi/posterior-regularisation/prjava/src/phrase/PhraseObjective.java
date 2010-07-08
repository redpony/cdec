package phrase;

import java.io.PrintStream;
import java.util.Arrays;
import java.util.List;

import optimization.gradientBasedMethods.ProjectedGradientDescent;
import optimization.gradientBasedMethods.ProjectedObjective;
import optimization.gradientBasedMethods.stats.OptimizerStats;
import optimization.linesearch.ArmijoLineSearchMinimizationAlongProjectionArc;
import optimization.linesearch.InterpolationPickFirstStep;
import optimization.linesearch.LineSearchMethod;
import optimization.linesearch.WolfRuleLineSearch;
import optimization.projections.SimplexProjection;
import optimization.stopCriteria.CompositeStopingCriteria;
import optimization.stopCriteria.ProjectedGradientL2Norm;
import optimization.stopCriteria.StopingCriteria;
import optimization.stopCriteria.ValueDifference;
import optimization.util.MathUtils;

public class PhraseObjective extends ProjectedObjective
{
	static final double GRAD_DIFF = 0.00002;
	static double INIT_STEP_SIZE = 10;
	static double VAL_DIFF = 1e-6; // FIXME needs to be tuned
	static int ITERATIONS = 100;
	//private double c1=0.0001; // wolf stuff
	//private double c2=0.9;
	private static double lambda[][];
	private PhraseCluster c;
	
	/**@brief
	 *  for debugging purposes
	 */
	//public static PrintStream ps;
	
	/**@brief current phrase being optimzed*/
	public int phrase;

	/**@brief un-regularized posterior
	 * unnormalized
	 * p[edge][tag]
	*  P(tag|edge) \propto P(tag|phrase)P(context|tag)
	 */
	private double[][]p;

	/**@brief regularized posterior
	 * q[edge][tag] propto p[edge][tag]*exp(-lambda)
	 */
	private double q[][];
	private List<Corpus.Edge> data;
	
	/**@brief log likelihood of the associated phrase
	 * 
	 */
	private double loglikelihood;
	private SimplexProjection projection;
	
	double[] newPoint  ;
	
	private int n_param;
	
	/**@brief likelihood under p
	 * 
	 */
	public double llh;
	
	public PhraseObjective(PhraseCluster cluster, int phraseIdx){
		phrase=phraseIdx;
		c=cluster;
		data=c.c.getEdgesForPhrase(phrase);
		n_param=data.size()*c.K;
		
		if (lambda==null){
			lambda=new double[c.c.getNumPhrases()][];
		}
		
		if (lambda[phrase]==null){
			lambda[phrase]=new double[n_param];
		}
		
		parameters=lambda[phrase];
		newPoint  = new double[n_param];
		gradient = new double[n_param];
		initP();
		projection=new SimplexProjection(c.scalePT);
		q=new double [data.size()][c.K];

		setParameters(parameters);
	}

	private void initP(){
		p=new double[data.size()][];
		for(int edge=0;edge<data.size();edge++){
			p[edge]=c.posterior(data.get(edge));
			llh += data.get(edge).getCount() * Math.log(arr.F.l1norm(p[edge])); // Was bug here - count inside log!
			arr.F.l1normalize(p[edge]);
		}
	}
	
	@Override
	public void setParameters(double[] params) {
		super.setParameters(params);
		updateFunction();
	}
	
	private void updateFunction(){
		updateCalls++;
		loglikelihood=0;

		for(int tag=0;tag<c.K;tag++){
			for(int edge=0;edge<data.size();edge++){
				q[edge][tag]=p[edge][tag]*
					Math.exp(-parameters[tag*data.size()+edge]/data.get(edge).getCount());
			}
		}
	
		for(int edge=0;edge<data.size();edge++){
			loglikelihood+=data.get(edge).getCount() * Math.log(arr.F.l1norm(q[edge]));
			arr.F.l1normalize(q[edge]);
		}
		
		for(int tag=0;tag<c.K;tag++){
			for(int edge=0;edge<data.size();edge++){
				gradient[tag*data.size()+edge]=-q[edge][tag];
			}
		}
	}
	
	@Override
	public double[] projectPoint(double[] point) {
		double toProject[]=new double[data.size()];
		for(int tag=0;tag<c.K;tag++){
			for(int edge=0;edge<data.size();edge++){
				toProject[edge]=point[tag*data.size()+edge];
			}
			projection.project(toProject);
			for(int edge=0;edge<data.size();edge++){
				newPoint[tag*data.size()+edge]=toProject[edge];
			}
		}
		return newPoint;
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

	public double [][]posterior(){
		return q;
	}
	
	public void optimizeWithProjectedGradientDescent(){
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
		boolean succed = optimizer.optimize(this,stats,compositeStop);
//		System.out.println("Ended optimzation Projected Gradient Descent\n" + stats.prettyPrint(1));
		if(succed){
			//System.out.println("Ended optimization in " + optimizer.getCurrentIteration());
		}else{
			System.out.println("Failed to optimize");
		}
		lambda[phrase]=parameters;
		//	ps.println(Arrays.toString(parameters));
		
		//	for(int edge=0;edge<data.getSize();edge++){
		//	ps.println(Arrays.toString(q[edge]));
		//	}
		
	}
	
	public double KL_divergence()
	{
		return -loglikelihood + MathUtils.dotProduct(parameters, gradient);
	}
	
	public double loglikelihood()
	{
		return llh;
	}
	
	public double l1lmax()
	{
		double sum=0;
		for(int tag=0;tag<c.K;tag++){
			double max=0;
			for(int edge=0;edge<data.size();edge++){
				if(q[edge][tag]>max)
					max=q[edge][tag];
			}
			sum+=max;
		}
		return sum;
	}

	public double primal()
	{
		return loglikelihood() - KL_divergence() - c.scalePT * l1lmax();	
	}
}
