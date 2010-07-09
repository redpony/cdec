package optimization.gradientBasedMethods;

import optimization.gradientBasedMethods.stats.OptimizerStats;
import optimization.stopCriteria.StopingCriteria;

public interface Optimizer {
	public boolean optimize(Objective o,OptimizerStats stats, StopingCriteria stoping);
	
	
	public double[] getDirection();
	public double getCurrentStep();
	public double getCurrentValue();
	public int getCurrentIteration();
	public void reset();
	
	public void setMaxIterations(int max);
	
		
}
