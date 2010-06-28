package io;
import java.util.*;
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
	public static PrintStream  openOutFile(String filename){
		PrintStream localps=null;
		try
		{
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
