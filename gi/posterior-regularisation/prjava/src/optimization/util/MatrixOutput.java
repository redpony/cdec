package optimization.util;


public class MatrixOutput {
	public static void printDoubleArray(double[][] array, String arrayName) {
		int size1 = array.length;
		int size2 = array[0].length;
		System.out.println(arrayName);
		for (int i = 0; i < size1; i++) {
			for (int j = 0; j < size2; j++) {
				System.out.print(" " + StaticTools.prettyPrint(array[i][j],
						"00.00E00", 4) + " ");

			}
			System.out.println();
		}
		System.out.println();
	}
	
	public static void printDoubleArray(double[] array, String arrayName) {
		System.out.println(arrayName);
		for (int i = 0; i < array.length; i++) {
				System.out.print(" " + StaticTools.prettyPrint(array[i],
						"00.00E00", 4) + " ");
		}
		System.out.println();
	}
}
