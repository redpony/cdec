package optimization.gradientBasedMethods;

import optimization.gradientBasedMethods.stats.OptimizerStats;
import optimization.linesearch.DifferentiableLineSearchObjective;
import optimization.linesearch.LineSearchMethod;
import optimization.stopCriteria.StopingCriteria;
import optimization.util.MathUtils;

/**
 * 
 * @author javg
 *
 */
public abstract class AbstractGradientBaseMethod implements Optimizer{
	
	protected int maxNumberOfIterations=10000;
	
	
	
	protected int currentProjectionIteration;
	protected double currValue;	
	protected double previousValue = Double.MAX_VALUE;;
	protected double step;
	protected double[] gradient;
	public double[] direction;
	
	//Original values
	protected double originalGradientL2Norm;
	
	protected LineSearchMethod lineSearch;
	DifferentiableLineSearchObjective lso;
	
	
	public void reset(){
		direction = null;
		gradient = null;
		previousValue = Double.MAX_VALUE;
		currentProjectionIteration = 0;
		originalGradientL2Norm = 0;
		step = 0;
		currValue = 0;
	}
	
	public void initializeStructures(Objective o,OptimizerStats stats, StopingCriteria stop){
		lso =   new DifferentiableLineSearchObjective(o);
	}
	public void updateStructuresBeforeStep(Objective o,OptimizerStats stats, StopingCriteria stop){
	}
	
	public void updateStructuresAfterStep(Objective o,OptimizerStats stats, StopingCriteria stop){
	}
	
	public boolean optimize(Objective o,OptimizerStats stats, StopingCriteria stop){
		//Initialize structures
			
		stats.collectInitStats(this, o);
		direction = new double[o.getNumParameters()];
		initializeStructures(o, stats, stop);
		for (currentProjectionIteration = 1; currentProjectionIteration < maxNumberOfIterations; currentProjectionIteration++){		
//			System.out.println("starting iterations: parameters:" );
//			o.printParameters();
			previousValue = currValue;
			currValue = o.getValue();
			gradient = o.getGradient();
			if(stop.stopOptimization(o)){
				stats.collectFinalStats(this, o);
				return true;
			}	
			
			getDirection();
			if(MathUtils.dotProduct(gradient, direction) > 0){
				System.out.println("Not a descent direction");
				System.out.println(" current stats " + stats.prettyPrint(1));
				System.exit(-1);
			}
			updateStructuresBeforeStep(o, stats, stop);
			lso.reset(direction);
			step = lineSearch.getStepSize(lso);
//			System.out.println("Leave with step: " + step);
			if(step==-1){
				System.out.println("Failed to find step");
				stats.collectFinalStats(this, o);
				return false;		
			}
			updateStructuresAfterStep( o, stats,  stop);
//			previousValue = currValue;
//			currValue = o.getValue();
//			gradient = o.getGradient();
			stats.collectIterationStats(this, o);
		}
		stats.collectFinalStats(this, o);
		return false;
	}
	
	
	public int getCurrentIteration() {
		return currentProjectionIteration;
	}

	
	/**
	 * Method specific
	 */
	public abstract double[] getDirection();

	public double getCurrentStep() {
		return step;
	}



	public void setMaxIterations(int max) {
		maxNumberOfIterations = max;
	}

	public double getCurrentValue() {
		return currValue;
	}
}
