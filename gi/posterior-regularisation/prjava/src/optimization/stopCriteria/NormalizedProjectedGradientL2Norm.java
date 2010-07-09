package optimization.stopCriteria;

import optimization.gradientBasedMethods.Objective;
import optimization.gradientBasedMethods.ProjectedObjective;
import optimization.util.MathUtils;

/**
 * Divides the norm by the norm at the begining of the iteration
 * @author javg
 *
 */
public class NormalizedProjectedGradientL2Norm extends ProjectedGradientL2Norm{
	
	/**
	 * Stop if gradientNorm/(originalGradientNorm) smaller
	 * than gradientConvergenceValue
	 */
	double originalProjectedNorm = -1;
	
	public NormalizedProjectedGradientL2Norm(double gradientConvergenceValue){
		super(gradientConvergenceValue);
	}
	
	public void reset(){
		originalProjectedNorm = -1;
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
			if(originalProjectedNorm == -1){
				originalProjectedNorm = norm;
			}
			double normalizedNorm = 1.0*norm/originalProjectedNorm;
			if( normalizedNorm < gradientConvergenceValue){
				System.out.println("Gradient norm below normalized normtreshold: " + norm + " original: " + originalProjectedNorm + " normalized norm: " + normalizedNorm);
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
