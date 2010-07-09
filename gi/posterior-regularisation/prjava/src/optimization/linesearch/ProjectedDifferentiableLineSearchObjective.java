package optimization.linesearch;

import optimization.gradientBasedMethods.Objective;
import optimization.gradientBasedMethods.ProjectedObjective;
import optimization.util.MathUtils;
import optimization.util.MatrixOutput;


/**
 * See ArmijoLineSearchMinimizationAlongProjectionArc for description
 * @author javg
 *
 */
public class ProjectedDifferentiableLineSearchObjective extends DifferentiableLineSearchObjective{

	
	
	ProjectedObjective obj;
	public ProjectedDifferentiableLineSearchObjective(Objective o) {
		super(o);
		if(!(o instanceof ProjectedObjective)){
			System.out.println("Must receive a projected objective");
			throw new RuntimeException();
		}
		obj = (ProjectedObjective) o;
	}

	
	
	public double[] projectPoint (double[] point){
		return ((ProjectedObjective)o).projectPoint(point);
	}
	public void updateAlpha(double alpha){
		if(alpha < 0){
			System.out.println("alpha may not be smaller that zero");
			throw new RuntimeException();
		}
		
		if(obj.auxParameters == null){
			obj.auxParameters = new double[obj.getParameters().length];
		}
		
		nrIterations++;
		
		steps.add(alpha);		
		System.arraycopy(originalParameters, 0, obj.auxParameters, 0, obj.auxParameters.length);
		
		//Take a step into the search direction
		
//		MatrixOutput.printDoubleArray(obj.getGradient(), "gradient");
		
//		alpha=gradients.get(0)*alpha/(gradients.get(gradients.size()-1));
	
		//x_t+1 = x_t - alpha*gradient = x_t + alpha*direction
		MathUtils.plusEquals(obj.auxParameters, searchDirection, alpha);
//		MatrixOutput.printDoubleArray(obj.auxParameters, "before projection");
		obj.auxParameters = projectPoint(obj.auxParameters);
//		MatrixOutput.printDoubleArray(obj.auxParameters, "after projection");
		o.setParameters(obj.auxParameters);
//		System.out.println("new parameters");
//		o.printParameters();
		values.add(o.getValue());
		//Computes the new gradient x_k-[x_k-alpha*Gradient(x_k)]+ 
		MathUtils.minusEqualsInverse(originalParameters,obj.auxParameters,1);
//		MatrixOutput.printDoubleArray(obj.auxParameters, "new gradient");
		//Dot product between the new direction and the new gradient
		double gradient = MathUtils.dotProduct(obj.auxParameters,searchDirection);
		gradients.add(gradient);	
		if(gradient > 0){
			System.out.println("Gradient on line search has to be smaller than zero");
			System.out.println("Iter: " + nrIterations);
			MatrixOutput.printDoubleArray(obj.auxParameters, "new direction");
			MatrixOutput.printDoubleArray(searchDirection, "search direction");
			throw new RuntimeException();
			
		}
		
	}
	
	/**
	 * 
	 */
//	public void updateAlpha(double alpha){
//		
//		if(alpha < 0){
//			System.out.println("alpha may not be smaller that zero");
//			throw new RuntimeException();
//		}
//		
//		nrIterations++;
//		steps.add(alpha);
//		//x_t+1 = x_t - alpha*direction
//		System.arraycopy(originalParameters, 0, parametersChange, 0, parametersChange.length);
////		MatrixOutput.printDoubleArray(parametersChange, "parameters before step");
////		System.out.println("Step" + alpha);
//		MatrixOutput.printDoubleArray(originalGradient, "gradient + " + alpha);
//
//		MathUtils.minusEquals(parametersChange, originalGradient, alpha);
//		
//		//Project the points into the feasibility set
////		MatrixOutput.printDoubleArray(parametersChange, "before projection");
//		//x_k(alpha) = [x_k - alpha*grad f(x_k)]+
//		parametersChange = projectPoint(parametersChange);
////		MatrixOutput.printDoubleArray(parametersChange, "after projection");
//		o.setParameters(parametersChange);
//		values.add(o.getValue());
//		//Computes the new direction x_k-[x_k-alpha*Gradient(x_k)]+
//		
//		direction=MathUtils.arrayMinus(parametersChange,originalParameters);
////		MatrixOutput.printDoubleArray(direction, "new direction");
//		
//		double gradient = MathUtils.dotProduct(originalGradient,direction);
//		gradients.add(gradient);		
//		if(gradient > 1E-10){
//			System.out.println("cosine " + gradient/(MathUtils.L2Norm(originalGradient)*MathUtils.L2Norm(direction)));
//			
//			
//			System.out.println("not a descent direction for alpha " + alpha);
//			System.arraycopy(originalParameters, 0, parametersChange, 0, parametersChange.length);
//			MathUtils.minusEquals(parametersChange, originalGradient, 1E-20);
//			
//			parametersChange = projectPoint(parametersChange);
//			direction=MathUtils.arrayMinus(parametersChange,originalParameters);
//			gradient = MathUtils.dotProduct(originalGradient,direction);
//			if(gradient > 0){
//				System.out.println("Direction is really non-descent evern for small alphas:" + gradient);
//			}
//			System.out.println("ProjecteLineSearchObjective: Should be a descent direction at " + nrIterations + ": "+ gradient);
////			System.out.println(Printing.doubleArrayToString(originalGradient, null,"Original gradient"));
////			System.out.println(Printing.doubleArrayToString(originalParameters, null,"Original parameters"));
////			System.out.println(Printing.doubleArrayToString(parametersChange, null,"Projected parameters"));
////			System.out.println(Printing.doubleArrayToString(direction, null,"Direction"));
//			throw new RuntimeException();
//		}
//	}
	
}
