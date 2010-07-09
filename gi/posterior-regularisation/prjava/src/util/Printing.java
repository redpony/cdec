package util;

public class Printing {
	static java.text.DecimalFormat fmt = new java.text.DecimalFormat();

	public static String padWithSpace(String s, int len){
		StringBuffer sb = new StringBuffer();
		while(sb.length() +s.length() < len){
			sb.append(" ");
		}
		sb.append(s);
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
	
	public static  String formatTime(long duration) {
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
	
	
	public static String doubleArrayToString(double[] array, String[] labels, String arrayName) {
		StringBuffer res = new StringBuffer();
		res.append(arrayName);
		res.append("\n");
		for (int i = 0; i < array.length; i++) {
			if (labels == null){
				res.append(i+"       \t");
			}else{
				res.append(labels[i]+     "\t");
			}
		}
		res.append("sum\n");
		double sum = 0;
		for (int i = 0; i < array.length; i++) {
			res.append(prettyPrint(array[i],
					"0.00000E00", 8) + "\t");
			sum+=array[i];
		}
		res.append(prettyPrint(sum,
				"0.00000E00", 8)+"\n");
		return res.toString();
	}
	
	
	
	public static void printDoubleArray(double[] array, String labels[], String arrayName) {
		System.out.println(doubleArrayToString(array, labels,arrayName));
	}
	
	
	public static String doubleArrayToString(double[][] array, String[] labels1, String[] labels2,
			String arrayName){
		StringBuffer res = new StringBuffer();
		res.append(arrayName);
		res.append("\n\t");
		//Calculates the column sum to keeps the sums
		double[] sums = new double[array[0].length+1];
		//Prints rows headings
		for (int i = 0; i < array[0].length; i++) {
			if (labels1 == null){
				res.append(i+"        \t");
			}else{
				res.append(labels1[i]+"        \t");
			}
		}
		res.append("sum\n");
		double sum = 0;
		//For each row print heading
		for (int i = 0; i < array.length; i++) {
			if (labels2 == null){
				res.append(i+"\t");
			}else{
				res.append(labels2[i]+"\t");
			}
			//Print values for that row
			for (int j = 0; j < array[0].length; j++) {
				res.append(" " + prettyPrint(array[i][j],
						"0.00000E00", 8) + "\t");
				sums[j] += array[i][j]; 
				sum+=array[i][j]; //Sum all values of that row
			}
			//Print row sum
			res.append(prettyPrint(sum,"0.00000E00", 8)+"\n");
			sums[array[0].length]+=sum;
			sum=0;
		}
		res.append("sum\t");
		//Print values for colums sum
		for (int i = 0; i < array[0].length+1; i++) {
			res.append(prettyPrint(sums[i],"0.00000E00", 8)+"\t");
		}
		res.append("\n");
		return res.toString();
	}
	
	public static void printDoubleArray(double[][] array, String[] labels1, String[] labels2
			, String arrayName) {
		System.out.println(doubleArrayToString(array, labels1,labels2,arrayName));
	}
	
	
	public static void printIntArray(int[][] array, String[] labels1, String[] labels2, String arrayName,
			int size1, int size2) {
		System.out.println(arrayName);
		for (int i = 0; i < size1; i++) {
			for (int j = 0; j < size2; j++) {
				System.out.print(" " + array[i][j] +  " ");

			}
			System.out.println();
		}
		System.out.println();
	}
	
	public static String intArrayToString(int[] array, String[] labels, String arrayName) {
		StringBuffer res = new StringBuffer();
		res.append(arrayName);
		for (int i = 0; i < array.length; i++) {
			res.append(" " + array[i] + " ");
			
		}
		res.append("\n");
		return res.toString();
	}
	
	public static void printIntArray(int[] array, String[] labels, String arrayName) {
		System.out.println(intArrayToString(array, labels,arrayName));
	}
	
	public static String toString(double[][] d){
		StringBuffer sb = new StringBuffer();
		for (int i = 0; i < d.length; i++) {
			for (int j = 0; j < d[0].length; j++) {
				sb.append(prettyPrint(d[i][j], "0.00E0", 10));
			}
			sb.append("\n");
		}
		return sb.toString();
	}
	
}
