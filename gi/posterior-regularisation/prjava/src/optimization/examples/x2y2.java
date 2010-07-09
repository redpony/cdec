package optimization.examples;


import optimization.gradientBasedMethods.ConjugateGradient;

import optimization.gradientBasedMethods.GradientDescent;
import optimization.gradientBasedMethods.LBFGS;
import optimization.gradientBasedMethods.Objective;
import optimization.gradientBasedMethods.stats.OptimizerStats;
import optimization.linesearch.GenericPickFirstStep;
import optimization.linesearch.LineSearchMethod;
import optimization.linesearch.WolfRuleLineSearch;
import optimization.stopCriteria.GradientL2Norm;
import optimization.stopCriteria.StopingCriteria;


/**
 * @author javg
 *
 */
public class x2y2 extends Objective{

	
	//Implements function ax2+ by2 
	double a, b;
	public x2y2(double a, double b){
		this.a = a;
		this.b = b;
		parameters = new double[2];
		parameters[0] = 4;
		parameters[1] = 4;
		gradient = new double[2];
	}
	
	public double getValue() {
		functionCalls++;
		return a*parameters[0]*parameters[0]+b*parameters[1]*parameters[1];
	}

	public double[] getGradient() {
		gradientCalls++;
		gradient[0]=2*a*parameters[0];
		gradient[1]=2*b*parameters[1];
		return gradient;
//		if(debugLevel >=2){
//			double[] numericalGradient = DebugHelpers.getNumericalGradient(this, parameters, 0.000001);
//			for(int i = 0; i < parameters.length; i++){
//				double diff = Math.abs(gradient[i]-numericalGradient[i]);
//				if(diff > 0.00001){
//					System.out.println("Numerical Gradient does not match");
//					System.exit(1);
//				}
//			}
//		}
	}

	
	
	public void optimizeWithGradientDescent(LineSearchMethod ls, OptimizerStats stats, x2y2 o){
		GradientDescent optimizer = new GradientDescent(ls);
		StopingCriteria stop = new GradientL2Norm(0.001);
//		optimizer.setGradientConvergenceValue(0.001);
		optimizer.setMaxIterations(100);
		boolean succed = optimizer.optimize(o,stats,stop);
		System.out.println("Ended optimzation Gradient Descent\n" + stats.prettyPrint(1));
		System.out.println("Solution: " + " x0 " + o.parameters[0]+ " x1 " + o.parameters[1]);
		if(succed){
			System.out.println("Ended optimization in " + optimizer.getCurrentIteration());
		}else{
			System.out.println("Failed to optimize");
		}
	}
	
	public void optimizeWithConjugateGradient(LineSearchMethod ls, OptimizerStats stats, x2y2 o){
		ConjugateGradient optimizer = new ConjugateGradient(ls);
		StopingCriteria stop = new GradientL2Norm(0.001);

		optimizer.setMaxIterations(10);
		boolean succed = optimizer.optimize(o,stats,stop);
		System.out.println("Ended optimzation Conjugate Gradient\n" + stats.prettyPrint(1));
		System.out.println("Solution: " + " x0 " + o.parameters[0]+ " x1 " + o.parameters[1]);
		if(succed){
			System.out.println("Ended optimization in " + optimizer.getCurrentIteration());
		}else{
			System.out.println("Failed to optimize");
		}
	}
	
	public void optimizeWithLBFGS(LineSearchMethod ls, OptimizerStats stats, x2y2 o){
		LBFGS optimizer = new LBFGS(ls,10);
		StopingCriteria stop = new GradientL2Norm(0.001);
		optimizer.setMaxIterations(10);
		boolean succed = optimizer.optimize(o,stats,stop);
		System.out.println("Ended optimzation LBFGS\n" + stats.prettyPrint(1));
		System.out.println("Solution: " + " x0 " + o.parameters[0]+ " x1 " + o.parameters[1]);
		if(succed){
			System.out.println("Ended optimization in " + optimizer.getCurrentIteration());
		}else{
			System.out.println("Failed to optimize");
		}
	}
	
	public static void main(String[] args) {
		x2y2 o = new x2y2(1,10);
		System.out.println("Starting optimization " + " x0 " + o.parameters[0]+ " x1 " + o.parameters[1]);
		o.setDebugLevel(4);
		LineSearchMethod wolfe = new WolfRuleLineSearch(new GenericPickFirstStep(1),0.001,0.9);;
//		LineSearchMethod ls = new ArmijoLineSearchMinimization();
		OptimizerStats stats = new OptimizerStats();
		o.optimizeWithGradientDescent(wolfe, stats, o);
		o = new x2y2(1,10);
		System.out.println("Starting optimization " + " x0 " + o.parameters[0]+ " x1 " + o.parameters[1]);
//		ls = new ArmijoLineSearchMinimization();
		stats = new OptimizerStats();
		o.optimizeWithConjugateGradient(wolfe, stats, o);
		o = new x2y2(1,10);
		System.out.println("Starting optimization " + " x0 " + o.parameters[0]+ " x1 " + o.parameters[1]);
//		ls = new ArmijoLineSearchMinimization();
		stats = new OptimizerStats();
		o.optimizeWithLBFGS(wolfe, stats, o);	
	}
	
	public String toString(){
		return "P1: " + parameters[0] + " P2: " + parameters[1] + " value " + getValue();
	}
	
	
}
