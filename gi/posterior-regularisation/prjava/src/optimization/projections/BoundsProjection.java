package optimization.projections;


import java.util.Random;

import optimization.util.MathUtils;
import optimization.util.MatrixOutput;

/**
 * Implements a projection into a box set defined by a and b.
 * If either a or b are infinity then that bound is ignored.
 * @author javg
 *
 */
public class BoundsProjection extends Projection{

	double a,b;
	boolean ignoreA = false;
	boolean ignoreB = false;
	public BoundsProjection(double lowerBound, double upperBound) {
		if(Double.isInfinite(lowerBound)){
			this.ignoreA = true;
		}else{
			this.a =lowerBound;
		}
		if(Double.isInfinite(upperBound)){
			this.ignoreB = true;
		}else{
			this.b =upperBound;
		}
	}
	
	
	
	/**
	* Projects into the bounds
	* a <= x_i <=b
	 */
	public void project(double[] original){
		for (int i = 0; i < original.length; i++) {
			if(!ignoreA && original[i] < a){
				original[i] = a;
			}else if(!ignoreB && original[i]>b){
				original[i]=b;
			}
		}
	}
	
	/**
	 * Generates a random number between a and b.
	 */

	Random r = new Random();
	
	public double[] samplePoint(int numParams) {
		double[] point = new double[numParams];
		for (int i = 0; i < point.length; i++) {
			double rand = r.nextDouble();
			if(ignoreA && ignoreB){
				//Use const to avoid number near overflow
				point[i] = rand*(1.E100+1.E100)-1.E100;
			}else if(ignoreA){
				point[i] = rand*(b-1.E100)-1.E100;
			}else if(ignoreB){
				point[i] = rand*(1.E100-a)-a;
			}else{
				point[i] = rand*(b-a)-a;
			}
		}
		return point;
	}
	
	public static void main(String[] args) {
		BoundsProjection sp = new BoundsProjection(0,Double.POSITIVE_INFINITY);
		
		
		MatrixOutput.printDoubleArray(sp.samplePoint(3), "random 1");
		MatrixOutput.printDoubleArray(sp.samplePoint(3), "random 2");
		MatrixOutput.printDoubleArray(sp.samplePoint(3), "random 3");
		
		double[] d = {-1.1,1.2,1.4};
		double[] original = d.clone();
		MatrixOutput.printDoubleArray(d, "before");
		
		sp.project(d);
		MatrixOutput.printDoubleArray(d, "after");
		System.out.println("Test projection: " + sp.testProjection(original, d));
	}
	
	double epsilon = 1.E-10;
	public double[] perturbePoint(double[] point, int parameter){
		double[] newPoint = point.clone();
		if(!ignoreA && MathUtils.almost(point[parameter], a)){
			newPoint[parameter]+=epsilon;
		}else if(!ignoreB && MathUtils.almost(point[parameter], b)){
			newPoint[parameter]-=epsilon;
		}else{
			newPoint[parameter]-=epsilon;
		}
		return newPoint;
	}

	
}
