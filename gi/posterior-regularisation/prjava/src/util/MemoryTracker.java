package util;


public class MemoryTracker {
	
	double initM,finalM;
	boolean start = false,finish = false;
	
	public MemoryTracker(){
		
	}
	
	public void start(){
		System.gc();
	    System.gc();
	    System.gc();
	    initM = (Runtime.getRuntime().totalMemory() - Runtime.getRuntime().freeMemory())/(1024*1024);  
	    start = true;
	}
	
	public void finish(){
		if(!start){
			throw new RuntimeException("Canot stop before starting");
		}
		System.gc();
	    System.gc();
	    System.gc();
	    finalM = (Runtime.getRuntime().totalMemory() - Runtime.getRuntime().freeMemory())/(1024*1024);  
	    finish = true;
	}
	
	public String print(){
		if(!finish){
			throw new RuntimeException("Canot print before stopping");
		}
		return "Used: " + (finalM - initM) + "MB";
	}
	
	public void clear(){
		initM = 0;
		finalM = 0;
		finish = false;
		start = false;
	}
	
	
}
