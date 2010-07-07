package io;
import java.util.*;
import java.util.zip.GZIPInputStream;
import java.util.zip.GZIPOutputStream;
import java.io.*;
public class FileUtil {
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
	
	public static BufferedReader openBufferedReader(String filename){
		BufferedReader r=null;
		try
		{
			if (filename.endsWith(".gz"))
				r=(new BufferedReader(new InputStreamReader(new GZIPInputStream(new FileInputStream(new File(filename))))));
			else
				r=(new BufferedReader(new FileReader(new File(filename))));
		}catch(IOException ioe){
			System.out.println(ioe.getMessage());
		}
		return r;		
	}
	
	public static PrintStream  openOutFile(String filename){
		PrintStream localps=null;
		try
		{
			if (filename.endsWith(".gz"))
				localps=new PrintStream (new GZIPOutputStream(new FileOutputStream(filename)));
			else
				localps=new PrintStream (new FileOutputStream(filename));

		}catch(IOException ioe){
			System.out.println(ioe.getMessage());
		}
		return localps;
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
