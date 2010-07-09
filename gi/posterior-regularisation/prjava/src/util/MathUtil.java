package util;

import java.util.Random;

public class MathUtil {
	public static final boolean closeToOne(double number){
		return Math.abs(number-1) < 1.E-10;
	}
	
	public static final boolean closeToZero(double number){
		return Math.abs(number) < 1.E-5;
	}
	
	/**
	 * Return a ramdom multinominal distribution.
	 * 
	 * @param size
	 * @return
	 */
	public static final double[] randomVector(int size, Random r){
		double[] random = new double[size];
		double sum=0;
		for(int i = 0; i < size; i++){
			double number = r.nextDouble();
			random[i] = number;
			sum+=number;
		}
		for(int i = 0; i < size; i++){
			random[i] = random[i]/sum;
		}
		return random;
	}
	
	

	public static double sum(double[] ds) {
		double res = 0;
		for (int i = 0; i < ds.length; i++) {
			res+=ds[i];
		}
		return res;
	}

	public static double max(double[] ds) {
		double res = Double.NEGATIVE_INFINITY;
		for (int i = 0; i < ds.length; i++) {
			res = Math.max(res, ds[i]);
		}
		return res;
	}

	public static double min(double[] ds) {
		double res = Double.POSITIVE_INFINITY;
		for (int i = 0; i < ds.length; i++) {
			res = Math.min(res, ds[i]);
		}
		return res;
	}

	
	public static double KLDistance(double[] p, double[] q) {
		int len = p.length;
		double kl = 0;
		for (int j = 0; j < len; j++) {
				if (p[j] == 0 || q[j] == 0) {
					continue;
				} else {
					kl += q[j] * Math.log(q[j] / p[j]);
				}

		}
		return kl;
	}
	
	public static double L2Distance(double[] p, double[] q) {
		int len = p.length;
		double l2 = 0;
		for (int j = 0; j < len; j++) {
				if (p[j] == 0 || q[j] == 0) {
					continue;
				} else {
					l2 += (q[j] - p[j])*(q[j] - p[j]);
				}

		}
		return Math.sqrt(l2);
	}
	
	public static double L1Distance(double[] p, double[] q) {
		int len = p.length;
		double l1 = 0;
		for (int j = 0; j < len; j++) {
				if (p[j] == 0 || q[j] == 0) {
					continue;
				} else {
					l1 += Math.abs(q[j] - p[j]);
				}

		}
		return l1;
	}

	public static double dot(double[] ds, double[] ds2) {
		double res = 0;
		for (int i = 0; i < ds2.length; i++) {
			res+= ds[i]*ds2[i];
		}
		return res;
	}
	
	public static double expDigamma(double number){
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

	public static double eulerGamma = 0.57721566490152386060651209008240243;
	// FIXME -- so far just the initialization from Minka's paper "Estimating a Dirichlet distribution". 
	public static double invDigamma(double y) {
		if (y>= -2.22) return Math.exp(y)+0.5;
		return -1.0/(y+eulerGamma);
	}

	
	
	public static void main(String[] args) {
		for(double i = 0; i < 10 ; i+=0.1){
			System.out.println(i+"\t"+expDigamma(i)+"\t"+(i-0.5));
		}
//		double gammaValue = (expDigamma(3)/expDigamma(10) + expDigamma(3)/expDigamma(10) + expDigamma(4)/expDigamma(10));
//		double normalValue = 3/10+3/4+10/10;
//		System.out.println("Gamma " + gammaValue + " normal " + normalValue);
	}

	
	
}
