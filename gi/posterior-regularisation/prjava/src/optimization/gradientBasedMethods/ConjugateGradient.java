package optimization.gradientBasedMethods;

import optimization.gradientBasedMethods.stats.OptimizerStats;
import optimization.linesearch.DifferentiableLineSearchObjective;
import optimization.linesearch.LineSearchMethod;
import optimization.stopCriteria.StopingCriteria;
import optimization.util.MathUtils;



public class ConjugateGradient extends AbstractGradientBaseMethod{
	
	
	double[] previousGradient;
	double[] previousDirection;

	public ConjugateGradient(LineSearchMethod lineSearch) {
		this.lineSearch = lineSearch;
	}
	
	public void reset(){
		super.reset();
		java.util.Arrays.fill(previousDirection, 0);
		java.util.Arrays.fill(previousGradient, 0);
	}
	
	public void initializeStructures(Objective o,OptimizerStats stats, StopingCriteria stop){
		super.initializeStructures(o, stats, stop);
		previousGradient = new double[o.getNumParameters()];
		previousDirection = new double[o.getNumParameters()];
	}
	public void updateStructuresBeforeStep(Objective o,OptimizerStats stats, StopingCriteria stop){
		System.arraycopy(gradient, 0, previousGradient, 0, gradient.length);
		System.arraycopy(direction, 0, previousDirection, 0, direction.length);	
	}
	
//	public boolean optimize(Objective o,OptimizerStats stats, StopingCriteria stop){
//		DifferentiableLineSearchObjective lso = new DifferentiableLineSearchObjective(o);
//		stats.collectInitStats(this, o);
//		direction = new double[o.getNumParameters()];
//		initializeStructures(o, stats, stop);
//		for (currentProjectionIteration = 0; currentProjectionIteration < maxNumberOfIterations; currentProjectionIteration++){
//			previousValue = currValue;
//			currValue = o.getValue();
//			gradient =o.getGradient();
//			if(stop.stopOptimization(gradient)){
//				stats.collectFinalStats(this, o);
//				return true;
//			}
//			getDirection();
//			updateStructures(o, stats, stop);
//			lso.reset(direction);
//			step = lineSearch.getStepSize(lso);	
//			if(step==-1){
//				System.out.println("Failed to find a step size");
//				System.out.println("Failed to find step");
//				stats.collectFinalStats(this, o);
//				return false;	
//			}
//			
//			stats.collectIterationStats(this, o);
//		}
//		stats.collectFinalStats(this, o);
//		return false;
//	}
	
	public double[] getDirection(){
		direction = MathUtils.negation(gradient);
		if(currentProjectionIteration != 1){
			//Using Polak-Ribiere method (book equation 5.45)
			double b = MathUtils.dotProduct(gradient, MathUtils.arrayMinus(gradient, previousGradient))
			/MathUtils.dotProduct(previousGradient, previousGradient);
			if(b<0){
				System.out.println("Defaulting to gradient descent");
				b = Math.max(b, 0);
			}
			MathUtils.plusEquals(direction, previousDirection, b);
			//Debug code
			if(MathUtils.dotProduct(direction, gradient) > 0){
				System.out.println("Not an descent direction reseting to gradien");
				direction = MathUtils.negation(gradient);
			}
		}
		return direction;
	}
	
	
	



}
