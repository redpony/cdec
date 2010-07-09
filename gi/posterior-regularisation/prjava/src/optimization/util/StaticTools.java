package optimization.util;


import java.io.File;
import java.io.PrintStream;

public class StaticTools {

	static java.text.DecimalFormat fmt = new java.text.DecimalFormat();

	public static void createDir(String directory) {

		File dir = new File(directory);
		if (!dir.isDirectory()) {
			boolean success = dir.mkdirs();
			if (!success) {
				System.out.println("Unable to create directory " + directory);
				System.exit(0);
			}
			System.out.println("Created directory " + directory);
		} else {
			System.out.println("Reusing directory " + directory);
		}
	}

	/*
	 * q and p are indexed by source/foreign Sum_S(q) = 1 the same for p KL(q,p) =
	 * Eq*q/p
	 */
	public static double KLDistance(double[][] p, double[][] q, int sourceSize,
			int foreignSize) {
		double totalKL = 0;
		// common.StaticTools.printMatrix(q, sourceSize, foreignSize, "q",
		// System.out);
		// common.StaticTools.printMatrix(p, sourceSize, foreignSize, "p",
		// System.out);
		for (int i = 0; i < sourceSize; i++) {
			double kl = 0;
			for (int j = 0; j < foreignSize; j++) {
				assert !Double.isNaN(q[i][j]) : "KLDistance q:  prob is NaN";
				assert !Double.isNaN(p[i][j]) : "KLDistance p:  prob is NaN";
				if (p[i][j] == 0 || q[i][j] == 0) {
					continue;
				} else {
					kl += q[i][j] * Math.log(q[i][j] / p[i][j]);
				}

			}
			totalKL += kl;
		}
		assert !Double.isNaN(totalKL) : "KLDistance: prob is NaN";
		if (totalKL < -1.0E-10) {
			System.out.println("KL Smaller than zero " + totalKL);
			System.out.println("Source Size" + sourceSize);
			System.out.println("Foreign Size" + foreignSize);
			StaticTools.printMatrix(q, sourceSize, foreignSize, "q",
					System.out);
			StaticTools.printMatrix(p, sourceSize, foreignSize, "p",
					System.out);
			System.exit(-1);
		}
		return totalKL / sourceSize;
	}

	/*
	 * indexed the by [fi][si]
	 */
	public static double KLDistancePrime(double[][] p, double[][] q,
			int sourceSize, int foreignSize) {
		double totalKL = 0;
		for (int i = 0; i < sourceSize; i++) {
			double kl = 0;
			for (int j = 0; j < foreignSize; j++) {
				assert !Double.isNaN(q[j][i]) : "KLDistance q:  prob is NaN";
				assert !Double.isNaN(p[j][i]) : "KLDistance p:  prob is NaN";
				if (p[j][i] == 0 || q[j][i] == 0) {
					continue;
				} else {
					kl += q[j][i] * Math.log(q[j][i] / p[j][i]);
				}

			}
			totalKL += kl;
		}
		assert !Double.isNaN(totalKL) : "KLDistance: prob is NaN";
		return totalKL / sourceSize;
	}

	public static double Entropy(double[][] p, int sourceSize, int foreignSize) {
		double totalE = 0;
		for (int i = 0; i < foreignSize; i++) {
			double e = 0;
			for (int j = 0; j < sourceSize; j++) {
				e += p[i][j] * Math.log(p[i][j]);
			}
			totalE += e;
		}
		return totalE / sourceSize;
	}

	public static double[][] copyMatrix(double[][] original, int sourceSize,
			int foreignSize) {
		double[][] result = new double[sourceSize][foreignSize];
		for (int i = 0; i < sourceSize; i++) {
			for (int j = 0; j < foreignSize; j++) {
				result[i][j] = original[i][j];
			}
		}
		return result;
	}

	public static void printMatrix(double[][] matrix, int sourceSize,
			int foreignSize, String info, PrintStream out) {

		java.text.DecimalFormat fmt = new java.text.DecimalFormat();
		fmt.setMaximumFractionDigits(3);
		fmt.setMaximumIntegerDigits(3);
		fmt.setMinimumFractionDigits(3);
		fmt.setMinimumIntegerDigits(3);

		out.println(info);

		for (int i = 0; i < foreignSize; i++) {
			for (int j = 0; j < sourceSize; j++) {
				out.print(prettyPrint(matrix[j][i], ".00E00", 6) + " ");
			}
			out.println();
		}
		out.println();
		out.println();
	}

	public static void printMatrix(int[][] matrix, int sourceSize,
			int foreignSize, String info, PrintStream out) {

		out.println(info);
		for (int i = 0; i < foreignSize; i++) {
			for (int j = 0; j < sourceSize; j++) {
				out.print(matrix[j][i] + " ");
			}
			out.println();
		}
		out.println();
		out.println();
	}

	public static String formatTime(long duration) {
		StringBuilder sb = new StringBuilder();
		double d = duration / 1000;
		fmt.applyPattern("00");
		sb.append(fmt.format((int) (d / (60 * 60))) + ":");
		d -= ((int) d / (60 * 60)) * 60 * 60;
		sb.append(fmt.format((int) (d / 60)) + ":");
		d -= ((int) d / 60) * 60;
		fmt.applyPattern("00.0");
		sb.append(fmt.format(d));
		return sb.toString();
	}

	public static String prettyPrint(double d, String patt, int len) {
		fmt.applyPattern(patt);
		String s = fmt.format(d);
		while (s.length() < len) {
			s = " " + s;
		}
		return s;
	}
	
	
	public static long getUsedMemory(){
		System.gc();
		return (Runtime.getRuntime().totalMemory() - Runtime.getRuntime().freeMemory())/ (1024 * 1024);
	}
	
	public final static boolean compareDoubles(double d1, double d2){
		return Math.abs(d1-d2) <= 1.E-10;
	}
	
	
}
