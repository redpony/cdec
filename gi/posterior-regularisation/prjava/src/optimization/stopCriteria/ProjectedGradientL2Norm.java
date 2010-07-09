package optimization.stopCriteria;

import optimization.gradientBasedMethods.Objective;
import optimization.gradientBasedMethods.ProjectedObjective;
import optimization.util.MathUtils;

public class ProjectedGradientL2Norm implements StopingCriteria{
	
	/**
	 * Stop if gradientNorm/(originalGradientNorm) smaller
	 * than gradientConvergenceValue
	 */
	protected double gradientConvergenceValue;
	
	
	public ProjectedGradientL2Norm(double gradientConvergenceValue){
		this.gradientConvergenceValue = gradientConvergenceValue;
	}
	
	public void reset(){
		
	}
	
	 double[] projectGradient(ProjectedObjective obj){
		
		if(obj.auxParameters == null){
			obj.auxParameters = new double[obj.getNumParameters()];
		}
		System.arraycopy(obj.getParameters(), 0, obj.auxParameters, 0, obj.getNumParameters());
		MathUtils.minusEquals(obj.auxParameters, obj.gradient, 1);
		obj.auxParameters = obj.projectPoint(obj.auxParameters);
		MathUtils.minusEquals(obj.auxParameters,obj.getParameters(),1);
		return obj.auxParameters;
	}
	
	public boolean stopOptimization(Objective obj){
		if(obj instanceof ProjectedObjective) {
			ProjectedObjective o = (ProjectedObjective) obj;
			double norm = MathUtils.L2Norm(projectGradient(o));
			if(norm < gradientConvergenceValue){
	//			System.out.println("Gradient norm below treshold: " + norm);
				return true;
			}else{
//				System.out.println("projected gradient norm: " + norm);
				return false;
			}
		}
		System.out.println("Not a projected objective");
		throw new RuntimeException();
	}
}
