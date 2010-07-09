package io;
import java.util.*;
import java.util.zip.GZIPInputStream;
import java.util.zip.GZIPOutputStream;
import java.io.*;
public class FileUtil 
{
	public static BufferedReader reader(File file) throws FileNotFoundException, IOException
	{
		if (file.getName().endsWith(".gz"))
			return new BufferedReader(new InputStreamReader(new GZIPInputStream(new FileInputStream(file))));
		else
			return new BufferedReader(new FileReader(file));
	}
	
	public static PrintStream printstream(File file) throws FileNotFoundException, IOException
	{
		if (file.getName().endsWith(".gz"))
			return new PrintStream(new GZIPOutputStream(new FileOutputStream(file)));
		else
			return new PrintStream(new FileOutputStream(file));
	}

	public static Scanner openInFile(String filename){
		Scanner localsc=null;
		try
		{
			localsc=new Scanner (new FileInputStream(filename));

		}catch(IOException ioe){
			System.out.println(ioe.getMessage());
		}
		return localsc;
	}
	
	public static FileInputStream openInputStream(String infilename){
		FileInputStream fis=null;
		try {
			fis =(new FileInputStream(infilename));
			
		} catch (IOException ioe) {
			System.out.println(ioe.getMessage());
		}
		return fis;
	}
}
