package optimization.gradientBasedMethods;

import optimization.util.MathUtils;


/**
 * Computes a projected objective
 * When we tell it to set some parameters it automatically projects the parameters back into the simplex:
 * 
 * 
 * When we tell it to get the gradient in automatically returns the projected gradient:
 * @author javg
 *
 */
public abstract class ProjectedObjective extends Objective{
	
	public abstract double[] projectPoint (double[] point);
	
	public double[] auxParameters;
	
	
	public  void setInitialParameters(double[] params){
		setParameters(projectPoint(params));
	}
	
	
	
	
}
