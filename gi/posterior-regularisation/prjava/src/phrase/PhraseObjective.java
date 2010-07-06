package phrase;

import java.io.PrintStream;
import java.util.Arrays;

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

public class PhraseObjective extends ProjectedObjective{

	private static final double GRAD_DIFF = 0.002;
	public static double INIT_STEP_SIZE=1;
	public static double VAL_DIFF=0.001;
	private double c1=0.0001;
	private double c2=0.9;
	
	private PhraseCluster c;
	
	/**@brief
	 *  for debugging purposes
	 */
	public static PrintStream ps;
	
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
	private int data[][];
	
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
	private double llh;
	
	public PhraseObjective(PhraseCluster cluster, int phraseIdx){
		phrase=phraseIdx;
		c=cluster;
		data=c.c.data[phrase];
		n_param=data.length*c.K;
		parameters=new double [n_param];
		newPoint  = new double[n_param];
		gradient = new double[n_param];
		initP();
		projection=new SimplexProjection(c.scale);
		q=new double [data.length][c.K];

		setParameters(parameters);
	}

	private void initP(){
		int countIdx=data[0].length-1;
		
		p=new double[data.length][];
		for(int edge=0;edge<data.length;edge++){
			p[edge]=c.posterior(phrase,data[edge]);
		}
		for(int edge=0;edge<data.length;edge++){
			llh+=Math.log
				(data[edge][countIdx]*arr.F.l1norm(p[edge]));
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
		int countIdx=data[0].length-1;
		for(int tag=0;tag<c.K;tag++){
			for(int edge=0;edge<data.length;edge++){
				q[edge][tag]=p[edge][tag]*
					Math.exp(-parameters[tag*data.length+edge]/data[edge][countIdx]);
			}
		}
	
		for(int edge=0;edge<data.length;edge++){
			loglikelihood+=data[edge][countIdx] * Math.log(arr.F.l1norm(q[edge]));
			arr.F.l1normalize(q[edge]);
		}
		
		for(int tag=0;tag<c.K;tag++){
			for(int edge=0;edge<data.length;edge++){
				gradient[tag*data.length+edge]=-q[edge][tag];
			}
		}
	}
	
	@Override
	// TODO Auto-generated method stub
	public double[] projectPoint(double[] point) {
		double toProject[]=new double[data.length];
		for(int tag=0;tag<c.K;tag++){
			for(int edge=0;edge<data.length;edge++){
				toProject[edge]=point[tag*data.length+edge];
			}
			projection.project(toProject);
			for(int edge=0;edge<data.length;edge++){
				newPoint[tag*data.length+edge]=toProject[edge];
			}
		}
		return newPoint;
	}

	@Override
	public double[] getGradient() {
		// TODO Auto-generated method stub
		gradientCalls++;
		return gradient;
	}

	@Override
	public double getValue() {
		// TODO Auto-generated method stub
		functionCalls++;
		return loglikelihood;
	}

	@Override
	public String toString() {
		// TODO Auto-generated method stub
		return "";
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
		StopingCriteria stopValue = new ValueDifference(VAL_DIFF);
		CompositeStopingCriteria compositeStop = new CompositeStopingCriteria();
		compositeStop.add(stopGrad);
		compositeStop.add(stopValue);
		optimizer.setMaxIterations(100);
		updateFunction();
		boolean succed = optimizer.optimize(this,stats,compositeStop);
//		System.out.println("Ended optimzation Projected Gradient Descent\n" + stats.prettyPrint(1));
		if(succed){
			System.out.println("Ended optimization in " + optimizer.getCurrentIteration());
		}else{
			System.out.println("Failed to optimize");
		}
		
		//	ps.println(Arrays.toString(parameters));
		
		//	for(int edge=0;edge<data.length;edge++){
		//	ps.println(Arrays.toString(q[edge]));
		//	}
		
	}
	
	/**
	 * L - KL(q||p) -
	 * 	 scale * \sum_{tag,phrase} max_i P(tag|i th occurrence of phrase)
	 * @return
	 */
	public double primal()
	{
		
		double l=llh;
		
//		ps.print("Phrase "+phrase+": "+l);
		double kl=-loglikelihood
			+MathUtils.dotProduct(parameters, gradient);
//		ps.print(", "+kl);
		l=l-kl;
		double sum=0;
		for(int tag=0;tag<c.K;tag++){
			double max=0;
			for(int edge=0;edge<data.length;edge++){
				if(q[edge][tag]>max){
					max=q[edge][tag];
				}
			}
			sum+=max;
		}
//		ps.println(", "+sum);
		l=l-c.scale*sum;
		return l;
	}
	
}
