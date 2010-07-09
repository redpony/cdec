package util;

import java.util.Arrays;

public class Array {

	
	
	public  static void sortDescending(double[] ds){
		for (int i = 0; i < ds.length; i++) ds[i] = -ds[i];
		Arrays.sort(ds);
		for (int i = 0; i < ds.length; i++) ds[i] = -ds[i];
	}
	
	/** 
	 * Return a new reversed array
	 * @param array
	 * @return
	 */
	public static int[] reverseIntArray(int[] array){
		int[] reversed = new int[array.length];
		for (int i = 0; i < reversed.length; i++) {
			reversed[i] = array[reversed.length-1-i];
		}
		return reversed;
	}
	
	public static String[] sumArray(String[] in, int from){
		String[] res = new String[in.length-from];
		for (int i = from; i < in.length; i++) {
			res[i-from] = in[i];
		}
		return res;
	}
	
	public static void main(String[] args) {
		int[] i = {1,2,3,4};
		util.Printing.printIntArray(i, null, "original");
		util.Printing.printIntArray(reverseIntArray(i), null, "reversed");
	}
}
