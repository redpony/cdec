package util;

import java.io.File;

public class FileSystem {
	public static boolean createDir(String directory) {

		File dir = new File(directory);
		if (!dir.isDirectory()) {
			boolean success = dir.mkdirs();
			if (!success) {
				System.out.println("Unable to create directory " + directory);
				return false;
			}
			System.out.println("Created directory " + directory);
		} else {
			System.out.println("Reusing directory " + directory);
		}
		return true;
	}
}
