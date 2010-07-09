package optimization.linesearch;

import optimization.util.Interpolation;


/**
 * Implements Back Tracking Line Search as described on page 37 of Numerical Optimization.
 * Also known as armijo rule
 * @author javg
 *
 */
public class ArmijoLineSearchMinimization implements LineSearchMethod{

	/**
	 * How much should the step size decrease at each iteration.
	 */
	double contractionFactor = 0.5;
	double c1 = 0.0001;
	
	double sigma1 = 0.1;
	double sigma2 = 0.9;


	
	double initialStep;
	int maxIterations = 10;
	
			
	public ArmijoLineSearchMinimization(){
		this.initialStep = 1;
	}
	
	//Experiment
	double previousStepPicked = -1;;
	double previousInitGradientDot = -1;
	double currentInitGradientDot = -1;
	
	
	public void reset(){
		previousStepPicked = -1;;
		previousInitGradientDot = -1;
		currentInitGradientDot = -1;
	}
	
	public void setInitialStep(double initial){
		initialStep = initial;
	}
	
	/**
	 * 
	 */
	
	public double getStepSize(DifferentiableLineSearchObjective o) {	
		currentInitGradientDot = o.getInitialGradient();
		//Should update all in the objective
		o.updateAlpha(initialStep);	
		int nrIterations = 0;
		//System.out.println("tried alpha" + initialStep + " value " + o.getCurrentValue());
		while(!WolfeConditions.suficientDecrease(o,c1)){			
			if(nrIterations >= maxIterations){
				o.printLineSearchSteps();	
				return -1;
			}
			double alpha=o.getAlpha();
			double alphaTemp = 
				Interpolation.quadraticInterpolation(o.getOriginalValue(), o.getInitialGradient(), alpha, o.getCurrentValue());
			if(alphaTemp >= sigma1 || alphaTemp <= sigma2*o.getAlpha()){
//				System.out.println("using alpha temp " + alphaTemp);
				alpha = alphaTemp;
			}else{
//				System.out.println("Discarding alpha temp " + alphaTemp);
				alpha = alpha*contractionFactor;
			}
//			double alpha =o.getAlpha()*contractionFactor;

			o.updateAlpha(alpha);
			//System.out.println("tried alpha" + alpha+ " value " + o.getCurrentValue());
			nrIterations++;			
		}
		
		//System.out.println("Leavning line search used:");
		//o.printLineSearchSteps();	
		
		previousInitGradientDot = currentInitGradientDot;
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
