package optimization.stopCriteria;

import optimization.gradientBasedMethods.Objective;

public interface StopingCriteria {
	public boolean stopOptimization(Objective obj);
	public void reset();
}
