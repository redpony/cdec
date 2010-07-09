package optimization.stopCriteria;

import optimization.gradientBasedMethods.Objective;
import optimization.gradientBasedMethods.ProjectedObjective;
import optimization.util.MathUtils;

/**
 * Divides the norm by the norm at the begining of the iteration
 * @author javg
 *
 */
public class NormalizedGradientL2Norm extends GradientL2Norm{
	
	/**
	 * Stop if gradientNorm/(originalGradientNorm) smaller
	 * than gradientConvergenceValue
	 */
	protected double originalGradientNorm = -1;
	
	public void reset(){
		originalGradientNorm = -1;
	}
	public NormalizedGradientL2Norm(double gradientConvergenceValue){
		super(gradientConvergenceValue);
	}
	
	
	 
	
	public boolean stopOptimization(Objective obj){
			double norm = MathUtils.L2Norm(obj.gradient);
			if(originalGradientNorm == -1){
				originalGradientNorm = norm;
			}
			if(originalGradientNorm < 1E-10){
				System.out.println("Gradient norm is zero " +  originalGradientNorm);
				return true;
			}
			double normalizedNorm = 1.0*norm/originalGradientNorm;
			if( normalizedNorm < gradientConvergenceValue){
				System.out.println("Gradient norm below normalized normtreshold: " + norm + " original: " + originalGradientNorm + " normalized norm: " + normalizedNorm);
				return true;
			}else{
//				System.out.println("projected gradient norm: " + norm);
				return false;
			}
	}
}
