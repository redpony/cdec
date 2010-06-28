package io;



import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.ObjectInput;
import java.io.ObjectInputStream;
import java.io.ObjectOutput;
import java.io.ObjectOutputStream;
import java.io.OutputStream;

public class SerializedObjects
{
	public static void writeSerializedObject(Object object, String outFile)
	{
		ObjectOutput output = null;
	    try{
	      //use buffering
	      OutputStream file = new FileOutputStream(outFile);
	      OutputStream buffer = new BufferedOutputStream( file );
	      output = new ObjectOutputStream( buffer );
	      output.writeObject(object);
	      buffer.close();
	      file.close();
	    }
	    catch(IOException ex){
	    	ex.printStackTrace();
	    }
	    finally{
	      try {
	        if (output != null) {
	          //flush and close "output" and its underlying streams
	          output.close();
	        }
	      }
	      catch (IOException ex ){
	    	  ex.printStackTrace();
	      }
	    }
	}
	
	public static Object readSerializedObject(String inputFile)
	{
		ObjectInput input = null;
	    Object recoveredObject=null;
		try{
	      //use buffering
	      InputStream file = new FileInputStream(inputFile);
	      InputStream buffer = new BufferedInputStream(file);
	      input = new ObjectInputStream(buffer);
	      //deserialize the List
	      recoveredObject = input.readObject();
	    }
	    catch(IOException ex){
	    	ex.printStackTrace();
	    }
	    catch (ClassNotFoundException ex){
	    	ex.printStackTrace();
	    }
	    catch(Exception ex)
	    {
	    	ex.printStackTrace();
	    }
	    finally{
	      try {
	        if ( input != null ) {
	          //close "input" and its underlying streams
	          input.close();
	        }
	      }
	      catch (IOException ex){
	    	  ex.printStackTrace();
	      }
	    }
	    return recoveredObject;
	 }
	
}