package util;

public class DigammaFunction {
	public static double expDigamma(double number){
		if(number==0)return number;
		return Math.exp(digamma(number));
	}
	
	public static double digamma(double number){
		if(number > 7){
			return digammApprox(number-0.5);
		}else{
			return digamma(number+1) - 1.0/number;
		}
	}
	
	private static double digammApprox(double value){
		return Math.log(value) + 0.04167*Math.pow(value, -2) - 0.00729*Math.pow(value, -4) 
		+  0.00384*Math.pow(value, -6) - 0.00413*Math.pow(value, -8);
	}
}
