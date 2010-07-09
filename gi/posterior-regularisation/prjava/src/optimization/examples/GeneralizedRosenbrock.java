package optimization.examples;


import optimization.gradientBasedMethods.ConjugateGradient;
import optimization.gradientBasedMethods.GradientDescent;
import optimization.gradientBasedMethods.LBFGS;
import optimization.gradientBasedMethods.Objective;
import optimization.gradientBasedMethods.Optimizer;
import optimization.gradientBasedMethods.stats.OptimizerStats;
import optimization.linesearch.ArmijoLineSearchMinimization;
import optimization.linesearch.LineSearchMethod;
import optimization.stopCriteria.GradientL2Norm;
import optimization.stopCriteria.StopingCriteria;
import optimization.util.MathUtils;

/**
 * 
 * @author javg
 * f(x) = \sum_{i=1}^{N-1} \left[ (1-x_i)^2+ 100 (x_{i+1} - x_i^2 )^2 \right] \quad \forall x\in\mathbb{R}^N.
 */
public class GeneralizedRosenbrock extends Objective{

	
	
	public GeneralizedRosenbrock(int dimensions){
		parameters = new double[dimensions];		
		java.util.Arrays.fill(parameters, 0);
		gradient = new double[dimensions];
		
	}
	
	public GeneralizedRosenbrock(int dimensions, double[] params){
		parameters = params;	
		gradient = new double[dimensions];
	}
	
	
	public double getValue() {
		functionCalls++;
		double value = 0;
		for(int i = 0; i < parameters.length-1; i++){
			value += MathUtils.square(1-parameters[i]) + 100*MathUtils.square(parameters[i+1] - MathUtils.square(parameters[i]));
		}
		
		return value;
	}

	/**
	 * gx = -2(1-x) -2x200(y-x^2)
	 * gy = 200(y-x^2)
	 */
	public double[] getGradient() {
		gradientCalls++;
		java.util.Arrays.fill(gradient,0);
		for(int i = 0; i < parameters.length-1; i++){
			gradient[i]+=-2*(1-parameters[i]) - 400*parameters[i]*(parameters[i+1] - MathUtils.square(parameters[i]));
			gradient[i+1]+=200*(parameters[i+1] - MathUtils.square(parameters[i]));
		}	
		return gradient;
	}

	

	

	
	
	public String toString(){
		String  res ="";
		for(int i = 0; i < parameters.length; i++){
			res += "P" + i+ " " + parameters[i];
		}
		res += " Value " + getValue();
		return res;
	}
	
	public static void main(String[] args) {
		
		GeneralizedRosenbrock o = new GeneralizedRosenbrock(2);
		System.out.println("Starting optimization " + " x0 " + o.parameters[0]+ " x1 " + o.parameters[1]);
		;

		System.out.println("Doing Gradient descent");
		//LineSearchMethod wolfe = new WolfRuleLineSearch(new InterpolationPickFirstStep(1),100,0.001,0.1);
		StopingCriteria stop = new GradientL2Norm(0.001);		
		LineSearchMethod ls = new ArmijoLineSearchMinimization();
		Optimizer optimizer = new GradientDescent(ls);		
		OptimizerStats stats = new OptimizerStats();
		optimizer.setMaxIterations(1000);
		boolean succed = optimizer.optimize(o,stats, stop);
		System.out.println("Suceess " + succed + "/n"+stats.prettyPrint(1));
		System.out.println("Doing Conjugate Gradient descent");
		o = new GeneralizedRosenbrock(2);
	//	wolfe = new WolfRuleLineSearch(new InterpolationPickFirstStep(1),100,0.001,0.1);
		optimizer = new ConjugateGradient(ls);
		stats = new OptimizerStats();
		optimizer.setMaxIterations(1000);
		succed = optimizer.optimize(o,stats,stop);
		System.out.println("Suceess " + succed + "/n"+stats.prettyPrint(1));
		System.out.println("Doing Quasi newton descent");
		o = new GeneralizedRosenbrock(2);
		optimizer = new LBFGS(ls,10);
		stats = new OptimizerStats();
		optimizer.setMaxIterations(1000);
		succed = optimizer.optimize(o,stats,stop);
		System.out.println("Suceess " + succed + "/n"+stats.prettyPrint(1));

	}
	
}
