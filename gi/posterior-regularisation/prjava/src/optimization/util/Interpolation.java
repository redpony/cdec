package optimization.util;

public class Interpolation {

	/**
	 * Fits a cubic polinomyal to a function given two points,
	 * such that either gradB is bigger than zero or funcB >= funcA
	 * 
	 * NonLinear Programming appendix C
	 * @param funcA
	 * @param gradA
	 * @param funcB
	 * @param gradB
	 */
	public final static double cubicInterpolation(double a, 
			double funcA, double gradA, double b,double funcB, double gradB ){
		if(gradB < 0 && funcA > funcB){
			System.out.println("Cannot call cubic interpolation");
			return -1;
		}
		
		double z = 3*(funcA-funcB)/(b-a) + gradA + gradB;
		double w = Math.sqrt(z*z - gradA*gradB);
		double min = b -(gradB+w-z)*(b-a)/(gradB-gradA+2*w);
		return min;
	}
	
	public final static double quadraticInterpolation(double initFValue, 
			double initGrad, double point,double pointFValue){
				double min = -1*initGrad*point*point/(2*(pointFValue-initGrad*point-initFValue));
		return min;
	}
	
	public static void main(String[] args) {
		
	}
}
