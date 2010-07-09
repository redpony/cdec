package optimization.linesearch;

import gnu.trove.TDoubleArrayList;
import gnu.trove.TIntArrayList;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Comparator;

import optimization.gradientBasedMethods.Objective;
import optimization.util.MathUtils;
import optimization.util.StaticTools;



import util.MathUtil;
import util.Printing;


/**
 * A wrapper class for the actual objective in order to perform 
 * line search.  The optimization code assumes that this does a lot 
 * of caching in order to simplify legibility.  For the applications 
 * we use it for, caching the entire history of evaluations should be 
 * a win. 
 * 
 * Note: the lastEvaluatedAt value is very important, since we will use
 * it to avoid doing an evaluation of the gradient after the line search.  
 * 
 * The differentiable line search objective defines a search along the ray
 * given by a direction of the main objective.
 * It defines the following function, 
 * where f is the original objective function:
 * g(alpha) = f(x_0 + alpha*direction)
 * g'(alpha) = f'(x_0 + alpha*direction)*direction
 * 
 * @author joao
 *
 */
public class DifferentiableLineSearchObjective {

	
	
	Objective o;
	int nrIterations;
	TDoubleArrayList steps;
	TDoubleArrayList values;
	TDoubleArrayList gradients;
	
	//This variables cannot change
	public double[] originalParameters;
	public double[] searchDirection;

	
	/**
	 * Defines a line search objective:
	 * Receives:
	 * Objective to each we are performing the line search, is used to calculate values and gradients
	 * Direction where to do the ray search, note that the direction does not depend of the 
	 * objective but depends from the method.
	 * @param o
	 * @param direction
	 */
	public DifferentiableLineSearchObjective(Objective o) {
		this.o = o;
		originalParameters = new double[o.getNumParameters()];
		searchDirection = new double[o.getNumParameters()];
		steps = new TDoubleArrayList();
		values = new TDoubleArrayList();
		gradients = new TDoubleArrayList();
	}
	/**
	 * Called whenever we start a new iteration. 
	 * Receives the ray where we are searching for and resets all values
	 * 
	 */
	public void reset(double[] direction){
		//Copy initial values
		System.arraycopy(o.getParameters(), 0, originalParameters, 0, o.getNumParameters());
		System.arraycopy(direction, 0, searchDirection, 0, o.getNumParameters());
		
		//Initialize variables
		nrIterations = 0;
		steps.clear();
		values.clear();
		gradients.clear();
	
		values.add(o.getValue());
		gradients.add(MathUtils.dotProduct(o.getGradient(),direction));	
		steps.add(0);
	}
	
	
	/**
	 * update the current value of alpha.
	 * Takes a step with that alpha in direction
	 * Get the real objective value and gradient and calculate all required information.
	 */
	public void updateAlpha(double alpha){
		if(alpha < 0){
			System.out.println("alpha may not be smaller that zero");
			throw new RuntimeException();
		}
		nrIterations++;
		steps.add(alpha);
		//x_t+1 = x_t + alpha*direction
		System.arraycopy(originalParameters,0, o.getParameters(), 0, originalParameters.length);
		MathUtils.plusEquals(o.getParameters(), searchDirection, alpha);
		o.setParameters(o.getParameters());
//		System.out.println("Took a step of " + alpha + " new value " + o.getValue());
		values.add(o.getValue());
		gradients.add(MathUtils.dotProduct(o.getGradient(),searchDirection));		
	}

	
	
	public int getNrIterations(){
		return nrIterations;
	}
	
	/**
	 * return g(alpha) for the current value of alpha
	 * @param iter
	 * @return
	 */
	public double getValue(int iter){
		return values.get(iter);
	}
	
	public double getCurrentValue(){
		return values.get(nrIterations);
	}
	
	public double getOriginalValue(){
		return values.get(0);
	}

	/**
	 * return g'(alpha) for the current value of alpha
	 * @param iter
	 * @return
	 */
	public double getGradient(int iter){
		return gradients.get(iter);
	}
	
	public double getCurrentGradient(){
		return gradients.get(nrIterations);
	}
	
	public double getInitialGradient(){
		return gradients.get(0);
	}
	
	
	
	
	public double getAlpha(){
		return steps.get(nrIterations);
	}
	
	public void printLineSearchSteps(){
		System.out.println(
				" Steps size "+steps.size() + 
				"Values size "+values.size() +
				"Gradeients size "+gradients.size());
		for(int i =0; i < steps.size();i++){
			System.out.println("Iter " + i + " step " + steps.get(i) +
					" value " + values.get(i) + " grad "  + gradients.get(i));
		}
	}
	
	public void printSmallLineSearchSteps(){
		for(int i =0; i < steps.size();i++){
			System.out.print(StaticTools.prettyPrint(steps.get(i), "0.0000E00",8) + " ");
		}
		System.out.println();
	}
	
	public static void main(String[] args) {
		
	}
	
}
