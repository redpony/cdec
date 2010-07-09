package util;

import java.io.BufferedReader;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.PrintStream;
import java.io.UnsupportedEncodingException;
import java.util.Properties;
import java.util.zip.GZIPInputStream;
import java.util.zip.GZIPOutputStream;

public class InputOutput {

	/**
	 * Opens a file either compress with gzip or not compressed.
	 */
	public static BufferedReader openReader(String fileName) throws UnsupportedEncodingException, FileNotFoundException, IOException{
		System.out.println("Reading: " + fileName);
		BufferedReader reader;
		fileName = fileName.trim();
		if(fileName.endsWith("gz")){
			reader = new BufferedReader(
			new InputStreamReader(new GZIPInputStream(new FileInputStream(fileName)),"UTF8"));
		}else{
			reader = new BufferedReader(new InputStreamReader(
					new FileInputStream(fileName), "UTF8"));
		}
		
		return reader;
	}
	
	
	public static PrintStream openWriter(String fileName) 
	throws UnsupportedEncodingException, FileNotFoundException, IOException{
		System.out.println("Writting to file: " + fileName);
		PrintStream writter;
		fileName = fileName.trim();
		if(fileName.endsWith("gz")){
			writter = new PrintStream(new GZIPOutputStream(new FileOutputStream(fileName)),
					true, "UTF-8");

		}else{
			writter = new PrintStream(new FileOutputStream(fileName),
					true, "UTF-8");

		}
		
		return writter;
	}
	
	public static Properties readPropertiesFile(String fileName) {
		Properties properties = new Properties();
		try {
			properties.load(new FileInputStream(fileName));
		} catch (IOException e) {
			e.printStackTrace();
			throw new AssertionError("Wrong properties file " + fileName);
		}
		System.out.println(properties.toString());
		
		return properties;
	}
}
