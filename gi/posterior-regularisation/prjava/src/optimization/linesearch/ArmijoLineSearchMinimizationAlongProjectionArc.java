package optimization.linesearch;

import optimization.gradientBasedMethods.ProjectedObjective;
import optimization.util.Interpolation;
import optimization.util.MathUtils;





/**
 * Implements Armijo Rule Line search along the projection arc (Non-Linear Programming page 230)
 * To be used with Projected gradient Methods.
 * 
 * Recall that armijo tries successive step sizes alpha until the sufficient decrease is satisfied:
 * f(x+alpha*direction) < f(x) + alpha*c1*grad(f)*direction
 * 
 * In this case we are optimizing over a convex set X so we must guarantee that the new point stays inside the 
 * constraints.
 * First the direction as to be feasible (inside constraints) and will be define as:
 * d = (x_k_f - x_k) where x_k_f is a feasible point.
 * so the armijo condition can be rewritten as:
 * f(x+alpha(x_k_f - x_k)) < f(x) + c1*grad(f)*(x_k_f - x_k)
 * and x_k_f is defined as:
 * [x_k-alpha*grad(f)]+
 * where []+ mean a projection to the feasibility set.
 * So this means that we take a step on the negative gradient (gradient descent) and then obtain then project
 * that point to the feasibility set. 
 * Note that if the point is already feasible then we are back to the normal armijo rule.
 * 
 * @author javg
 *
 */
public class ArmijoLineSearchMinimizationAlongProjectionArc implements LineSearchMethod{

	/**
	 * How much should the step size decrease at each iteration.
	 */
	double contractionFactor = 0.5;
	double c1 = 0.0001;
	
	
	double initialStep;
	int maxIterations = 100;
			
	
	double sigma1 = 0.1;
	double sigma2 = 0.9;
	
	//Experiment
	double previousStepPicked = -1;;
	double previousInitGradientDot = -1;
	double currentInitGradientDot = -1;
	
	GenericPickFirstStep strategy;
	
	
	public void reset(){
		previousStepPicked = -1;;
		previousInitGradientDot = -1;
		currentInitGradientDot = -1;
	}

	
	public ArmijoLineSearchMinimizationAlongProjectionArc(){
		this.initialStep = 1;
	}
	
	public ArmijoLineSearchMinimizationAlongProjectionArc(GenericPickFirstStep strategy){
		this.strategy = strategy;
		this.initialStep = strategy.getFirstStep(this);
	}
	
	
	public void setInitialStep(double initial){
		this.initialStep = initial;
	}
	
	/**
	 * 
	 */
	
	public double getStepSize(DifferentiableLineSearchObjective o) {	

		
		//Should update all in the objective
		initialStep = strategy.getFirstStep(this);
		o.updateAlpha(initialStep);	
		previousInitGradientDot=currentInitGradientDot;
		currentInitGradientDot=o.getCurrentGradient();
		int nrIterations = 0;
	
		//Armijo rule, the current value has to be smaller than the original value plus a small step of the gradient
		while(o.getCurrentValue()  >
			o.getOriginalValue() + c1*(o.getCurrentGradient())){			
//			System.out.println("curr value "+o.getCurrentValue());
//			System.out.println("original value "+o.getOriginalValue());
//			System.out.println("GRADIENT decrease" +(MathUtils.dotProduct(o.o.gradient,
//					MathUtils.arrayMinus(o.originalParameters,((ProjectedObjective)o.o).auxParameters))));
//			System.out.println("GRADIENT SAVED" + o.getCurrentGradient());
			if(nrIterations >= maxIterations){
				System.out.println("Could not find a step leaving line search with -1");
				o.printLineSearchSteps();
				return -1;
			}
			double alpha=o.getAlpha();
			double alphaTemp = 
				Interpolation.quadraticInterpolation(o.getOriginalValue(), o.getInitialGradient(), alpha, o.getCurrentValue());
			if(alphaTemp >= sigma1 || alphaTemp <= sigma2*o.getAlpha()){
				alpha = alphaTemp;
			}else{
				alpha = alpha*contractionFactor;
			}
//			double alpha =obj.getAlpha()*contractionFactor;
			o.updateAlpha(alpha);
			nrIterations++;			
		}
//		System.out.println("curr value "+o.getCurrentValue());
//		System.out.println("original value "+o.getOriginalValue());
//		System.out.println("sufficient decrease" +c1*o.getCurrentGradient());
//		System.out.println("Leavning line search used:");
//		o.printSmallLineSearchSteps();	
		
		previousStepPicked = o.getAlpha();
		return o.getAlpha();
	}
	
	public double getInitialGradient() {
		return currentInitGradientDot;
		
	}

	public double getPreviousInitialGradient() {
		return previousInitGradientDot;
	}

	public double getPreviousStepUsed() {
		return previousStepPicked;
	}
		
}
