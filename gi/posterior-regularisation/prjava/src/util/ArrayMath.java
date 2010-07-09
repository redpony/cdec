package util;

import java.util.Arrays;

public class ArrayMath {

	public static double dotProduct(double[] v1, double[] v2) {
		assert(v1.length == v2.length);
		double result = 0;
		for(int i = 0; i < v1.length; i++)
			result += v1[i]*v2[i];
		return result;
	}

	public static double twoNormSquared(double[] v) {
		double result = 0;
		for(double d : v)
			result += d*d;
		return result;
	}

	public static boolean containsInvalid(double[] v) {
		for(int i = 0; i < v.length; i++)
			if(Double.isNaN(v[i]) || Double.isInfinite(v[i]))
				return true;
		return false;
	}


	
	public static double safeAdd(double[] toAdd) {
		// Make sure there are no positive infinities
		double sum = 0;
		for(int i = 0; i < toAdd.length; i++) {
			assert(!(Double.isInfinite(toAdd[i]) && toAdd[i] > 0));
			assert(!Double.isNaN(toAdd[i]));
			sum += toAdd[i];
		}
		
		return sum;
	}

	/* Methods for filling integer and double arrays (of up to four dimensions) with the given value. */
	
	public static void set(int[][][][] array, int value) {
		for(int i = 0; i < array.length; i++) {
			set(array[i], value);
		}
	}
	
	public static void set(int[][][] array, int value) {
		for(int i = 0; i < array.length; i++) {
			set(array[i], value);
		}
	}
	
	public static void set(int[][] array, int value) {
		for(int i = 0; i < array.length; i++) {
			set(array[i], value);
		}
	}
	
	public static void set(int[] array, int value) {
		Arrays.fill(array, value);
	}
	
	
	public static void set(double[][][][] array, double value) {
		for(int i = 0; i < array.length; i++) {
			set(array[i], value);
		}
	}
	
	public static void set(double[][][] array, double value) {
		for(int i = 0; i < array.length; i++) {
			set(array[i], value);
		}
	}
	
	public static void set(double[][] array, double value) {
		for(int i = 0; i < array.length; i++) {
			set(array[i], value);
		}
	}
	
	public static void set(double[] array, double value) {
		Arrays.fill(array, value);
	}

	public static void setEqual(double[][][][] dest, double[][][][] source){
		for (int i = 0; i < source.length; i++) {
			setEqual(dest[i],source[i]);
		}
	}

	
	public static void setEqual(double[][][] dest, double[][][] source){
		for (int i = 0; i < source.length; i++) {
			set(dest[i],source[i]);
		}
	}

	
	public static void set(double[][] dest, double[][] source){
		for (int i = 0; i < source.length; i++) {
			setEqual(dest[i],source[i]);
		}
	}

	public static void setEqual(double[] dest, double[] source){
		System.arraycopy(source, 0, dest, 0, source.length);
	}

	public static void plusEquals(double[][][][] array, double val){
		for (int i = 0; i < array.length; i++) {
			plusEquals(array[i], val);
		}
	}	
	
	public static void plusEquals(double[][][] array, double val){
		for (int i = 0; i < array.length; i++) {
			plusEquals(array[i], val);
		}
	}	
	
	public static void plusEquals(double[][] array, double val){
		for (int i = 0; i < array.length; i++) {
			plusEquals(array[i], val);
		}
	}	
	
	public static void plusEquals(double[] array, double val){
		for (int i = 0; i < array.length; i++) {
			array[i] += val;
		}
	}

	
	public static double sum(double[] array) {
		double res = 0;
		for (int i = 0; i < array.length; i++) res += array[i];
		return res;
	}


	
	public static  double[][] deepclone(double[][] in){
		double[][] res = new double[in.length][];
		for (int i = 0; i < res.length; i++) {
			res[i] = in[i].clone();
		}
		return res;
	}

	
	public static  double[][][] deepclone(double[][][] in){
		double[][][] res = new double[in.length][][];
		for (int i = 0; i < res.length; i++) {
			res[i] = deepclone(in[i]);
		}
		return res;
	}

	public static double cosine(double[] a,
			double[] b) {
		return (dotProduct(a, b)+1e-5)/(Math.sqrt(dotProduct(a, a)+1e-5)*Math.sqrt(dotProduct(b, b)+1e-5));
	}

	public static double max(double[] ds) {
		double max = Double.NEGATIVE_INFINITY;
		for(double d:ds) max = Math.max(d,max);
		return max;
	}

	public static void exponentiate(double[] a) {
		for (int i = 0; i < a.length; i++) {
			a[i] = Math.exp(a[i]);
		}
	}

	public static int sum(int[] array) {
		int res = 0;
		for (int i = 0; i < array.length; i++) res += array[i];
		return res;
	}
}
