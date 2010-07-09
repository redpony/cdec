package optimization.projections;



import java.util.Random;

import optimization.util.MathUtils;
import optimization.util.MatrixOutput;

public class SimplexProjection extends Projection{

	double scale;
	public SimplexProjection(double scale) {
		this.scale = scale;
	}
	
	/**
	 * projects the numbers of the array 
	 * into a simplex of size. 
	 * We follow the description of the paper
	 * "Efficient Projetions onto the l1-Ball
	 * for learning in high dimensions"
	 */
	public void project(double[] original){
		double[] ds = new double[original.length];
		System.arraycopy(original, 0, ds, 0, ds.length);
		//If sum is smaller then zero then its ok
		for (int i = 0; i < ds.length; i++) ds[i] = ds[i]>0? ds[i]:0;
		double sum = MathUtils.sum(ds);
		if (scale - sum >=  -1.E-10 ){
			System.arraycopy(ds, 0, original, 0, ds.length);
			//System.out.println("Not projecting");
			return;
		}
		//System.out.println("projecting " + sum + " scontraints " + scale);	
		util.Array.sortDescending(ds);
		double currentSum = 0;
		double previousTheta = 0;
		double theta = 0;
		for (int i = 0; i < ds.length; i++) {
			currentSum+=ds[i];
			theta = (currentSum-scale)/(i+1);
			if(ds[i]-theta <= 0){
				break;
			}
			previousTheta = theta;
		}
		//DEBUG
		if(previousTheta < 0){
			System.out.println("Simple Projection: Theta is smaller than zero: " + previousTheta);
			System.exit(-1);
		}
		for (int i = 0; i < original.length; i++) {
			original[i] = Math.max(original[i]-previousTheta, 0);
		}
	}
	
	
	
	
	

	/**
	 * Samples a point from the simplex of scale. Just sample
	 * random number from 0-scale and then if
	 * their sum is bigger then sum make them normalize.
	 * This is probably not sampling uniformly from the simplex but it is
	 * enough for our goals in here.
	 */
	Random r = new Random();
	public double[] samplePoint(int dimensions) {
		double[] newPoint = new double[dimensions];
		double sum =0;
		for (int i = 0; i < newPoint.length; i++) {
			double rand = r.nextDouble()*scale;
			sum+=rand;
			newPoint[i]=rand;
		}
		//Normalize
		if(sum > scale){
			for (int i = 0; i < newPoint.length; i++) {
				newPoint[i]=scale*newPoint[i]/sum;
			}
		}
		return newPoint;
	}
	
	public static void main(String[] args) {
		SimplexProjection sp = new SimplexProjection(1);
		
		
		double[] point = sp.samplePoint(3);
		MatrixOutput.printDoubleArray(point , "random 1 sum:" + MathUtils.sum(point));
		point = sp.samplePoint(3);
		MatrixOutput.printDoubleArray(point , "random 2 sum:" + MathUtils.sum(point));
		point = sp.samplePoint(3);
		MatrixOutput.printDoubleArray(point , "random 3 sum:" + MathUtils.sum(point));
		
		double[] d = {0,1.1,-10};
		double[] original = d.clone();
		MatrixOutput.printDoubleArray(d, "before");
		
		sp.project(d);
		MatrixOutput.printDoubleArray(d, "after");
		System.out.println("Test projection: " + sp.testProjection(original, d));
		
	}
	
	
	double epsilon = 1.E-10;
	public double[] perturbePoint(double[] point, int parameter){
		double[] newPoint = point.clone();
		if(MathUtils.almost(MathUtils.sum(point), scale)){
			newPoint[parameter]-=epsilon;
		}
		else if(point[parameter]==0){
			newPoint[parameter]+=epsilon;
		}else if(MathUtils.almost(point[parameter], scale)){
			newPoint[parameter]-=epsilon;
		}
		else{
			newPoint[parameter]-=epsilon;
		}
		return newPoint;
	}
	
}
