package optimization.util;

import java.util.Arrays;



public class MathUtils {
	
	/**
	 * 
	 * @param vector
	 * @return
	 */
	public static double L2Norm(double[] vector){
		double value = 0;
		for(int i = 0; i < vector.length; i++){
			double v = vector[i];
			value+=v*v;
		}
		return Math.sqrt(value);
	}
	
	public static double sum(double[] v){
		double sum = 0;
		for (int i = 0; i < v.length; i++) {
			sum+=v[i];
		}
		return sum;
	}
	
	
	
	
	/**
	 * w = w + v
	 * @param w
	 * @param v
	 */
	public static void plusEquals(double[] w, double[] v) {
		for(int i=0; i<w.length;i++){
			w[i] += w[i] + v[i];
		}		
	}
	
	/**
	 * w[i] = w[i] + v
	 * @param w
	 * @param v
	 */
	public static void plusEquals(double[] w, double v) {
		for(int i=0; i<w.length;i++){
			w[i] += w[i] + v;
		}		
	}
	
	/**
	 * w[i] = w[i] - v
	 * @param w
	 * @param v
	 */
	public static void minusEquals(double[] w, double v) {
		for(int i=0; i<w.length;i++){
			w[i] -= w[i] + v;
		}		
	}
	
	/**
	 * w = w + a*v
	 * @param w
	 * @param v
	 * @param a
	 */
	public static void plusEquals(double[] w, double[] v, double a) {
		for(int i=0; i<w.length;i++){
			w[i] += a*v[i];
		}		
	}
	
	/**
	 * w = w - a*v
	 * @param w
	 * @param v
	 * @param a
	 */
	public static void minusEquals(double[] w, double[] v, double a) {
		for(int i=0; i<w.length;i++){
			w[i] -= a*v[i];
		}		
	}
	/**
	 * v = w - a*v
	 * @param w
	 * @param v
	 * @param a
	 */
	public static void minusEqualsInverse(double[] w, double[] v, double a) {
		for(int i=0; i<w.length;i++){
			v[i] = w[i] - a*v[i];
		}		
	}
	
	public static double dotProduct(double[] w, double[] v){
		double accum = 0;
		for(int i=0; i<w.length;i++){
			accum += w[i]*v[i];
		}
		return accum;
	}
	
	public static double[] arrayMinus(double[]w, double[]v){
		double result[] = w.clone();
		for(int i=0; i<w.length;i++){
			result[i] -= v[i];
		}
		return result;
	}
	
	public static double[] arrayMinus(double[] result , double[]w, double[]v){
		for(int i=0; i<w.length;i++){
			result[i] = w[i]-v[i];
		}
		return result;
	}
	
	public static double[] negation(double[]w){
		double result[]  = new double[w.length];
		for(int i=0; i<w.length;i++){
			result[i] = -w[i];
		}
		return result;
	}
	
	public static double square(double value){
		return value*value;
	}
	public static double[][] outerProduct(double[] w, double[] v){
		double[][] result = new double[w.length][v.length];
		for(int i = 0; i < w.length; i++){
			for(int j = 0; j < v.length; j++){
				result[i][j] = w[i]*v[j];
			}
		}
		return result;
	}
	/**
	 * results = a*W*V
	 * @param w
	 * @param v
	 * @param a
	 * @return
	 */
	public static double[][] weightedouterProduct(double[] w, double[] v, double a){
		double[][] result = new double[w.length][v.length];
		for(int i = 0; i < w.length; i++){
			for(int j = 0; j < v.length; j++){
				result[i][j] = a*w[i]*v[j];
			}
		}
		return result;
	}
	
	public static double[][] identity(int size){
		double[][] result = new double[size][size];
		for(int i = 0; i < size; i++){
			result[i][i] = 1;
		}
		return result;
	}
	
	/**
	 * v -= w
	 * @param v
	 * @param w
	 */
	public static void minusEquals(double[][] w, double[][] v){
		for(int i = 0; i < w.length; i++){
			for(int j = 0; j < w[0].length; j++){
				w[i][j] -= v[i][j];
			}
		}
	}
	
	/**
	 * v[i][j] -= a*w[i][j]
	 * @param v
	 * @param w
	 */
	public static void minusEquals(double[][] w, double[][] v, double a){
		for(int i = 0; i < w.length; i++){
			for(int j = 0; j < w[0].length; j++){
				w[i][j] -= a*v[i][j];
			}
		}
	}
	
	/**
	 * v += w
	 * @param v
	 * @param w
	 */
	public static void plusEquals(double[][] w, double[][] v){
		for(int i = 0; i < w.length; i++){
			for(int j = 0; j < w[0].length; j++){
				w[i][j] += v[i][j];
			}
		}
	}
	
	/**
	 * v[i][j] += a*w[i][j]
	 * @param v
	 * @param w
	 */
	public static void plusEquals(double[][] w, double[][] v, double a){
		for(int i = 0; i < w.length; i++){
			for(int j = 0; j < w[0].length; j++){
				w[i][j] += a*v[i][j];
			}
		}
	}
	
	
	/**
	 * results = w*v
	 * @param w
	 * @param v
	 * @return
	 */
	public static  double[][] matrixMultiplication(double[][] w,double[][] v){
		int w1 = w.length;
		int w2 = w[0].length;
		int v1 = v.length;
		int v2 = v[0].length;
		
		if(w2 != v1){
			System.out.println("Matrix dimensions do not agree...");
			System.exit(-1);
		}
		
		double[][] result = new double[w1][v2];
		for(int w_i1 = 0; w_i1 < w1; w_i1++){
			for(int v_i2 = 0; v_i2 < v2; v_i2++){
				double sum = 0;
				for(int w_i2 = 0; w_i2 < w2; w_i2++){
						sum += w[w_i1 ][w_i2]*v[w_i2][v_i2];	
				}
				result[w_i1][v_i2] = sum;
			}
		}
		return result;
	}
	
	/**
	 * w = w.*v
	 * @param w
	 * @param v
	 */
	public static  void matrixScalarMultiplication(double[][] w,double v){
		int w1 = w.length;
		int w2 = w[0].length;	
		for(int w_i1 = 0; w_i1 < w1; w_i1++){
				for(int w_i2 = 0; w_i2 < w2; w_i2++){
						w[w_i1 ][w_i2] *= v;	
				}
		}
	}
	
	public static  void scalarMultiplication(double[] w,double v){
		int w1 = w.length;
		for(int w_i1 = 0; w_i1 < w1; w_i1++){
			w[w_i1 ] *= v;	
		}
		
	}
	
	public static  double[] matrixVector(double[][] w,double[] v){
		int w1 = w.length;
		int w2 = w[0].length;
		int v1 = v.length;
		
		if(w2 != v1){
			System.out.println("Matrix dimensions do not agree...");
			System.exit(-1);
		}
		
		double[] result = new double[w1];
		for(int w_i1 = 0; w_i1 < w1; w_i1++){
				double sum = 0;
				for(int w_i2 = 0; w_i2 < w2; w_i2++){
						sum += w[w_i1 ][w_i2]*v[w_i2];	
				}
				result[w_i1] = sum;
		}
		return result;
	}
	
	public static boolean allPositive(double[] array){
		for (int i = 0; i < array.length; i++) {
			if(array[i] < 0) return false;
		}
		return true;
	}
	
	
	
	
	
		public static void main(String[] args) {
			double[][] m1 = new double[2][2];
			m1[0][0]=2;
			m1[1][0]=2;
			m1[0][1]=2;
			m1[1][1]=2;
			MatrixOutput.printDoubleArray(m1, "m1");
			double[][] m2 = new double[2][2];
			m2[0][0]=3;
			m2[1][0]=3;
			m2[0][1]=3;
			m2[1][1]=3;
			MatrixOutput.printDoubleArray(m2, "m2");
			double[][] result = matrixMultiplication(m1, m2);
			MatrixOutput.printDoubleArray(result, "result");
			matrixScalarMultiplication(result, 3);
			MatrixOutput.printDoubleArray(result, "result after multiply by 3");
		}
	
		public static boolean almost(double a, double b, double prec){
			return Math.abs(a-b)/Math.abs(a+b) <= prec || (almostZero(a) && almostZero(b));
		}

		public static boolean almost(double a, double b){
			return Math.abs(a-b)/Math.abs(a+b) <= 1e-10 || (almostZero(a) && almostZero(b));
		}

		public static boolean almostZero(double a) {
			return Math.abs(a) <= 1e-30;
		}
	
}
