package optimization.gradientBasedMethods.stats;

import java.util.ArrayList;

import optimization.gradientBasedMethods.Objective;
import optimization.gradientBasedMethods.Optimizer;
import optimization.util.MathUtils;
import optimization.util.StaticTools;


public class OptimizerStats {
	
	double start = 0;
	double totalTime = 0;
	
	String objectiveFinalStats;
	
	ArrayList<Double> gradientNorms = new ArrayList<Double>();
	ArrayList<Double> steps = new ArrayList<Double>();
	ArrayList<Double> value = new ArrayList<Double>();
	ArrayList<Integer> iterations = new ArrayList<Integer>();
	double prevValue =0;
	
	public void reset(){
		start = 0;
		totalTime = 0;
		
		objectiveFinalStats="";
		
		gradientNorms.clear();
		steps.clear();
		value.clear();
		iterations.clear();
		prevValue =0;
	}
	
	public void startTime() {
		start = System.currentTimeMillis();
	}
	public void stopTime() {
		totalTime += System.currentTimeMillis() - start;
	}
	
	public String prettyPrint(int level){
		StringBuffer res = new StringBuffer();
		res.append("Total time " + totalTime/1000 + " seconds \n" + "Iterations " + iterations.size() + "\n");
		res.append(objectiveFinalStats+"\n");
		if(level > 0){
			if(iterations.size() > 0){
			res.append("\tIteration"+iterations.get(0)+"\tstep: "+StaticTools.prettyPrint(steps.get(0), "0.00E00", 6)+ "\tgradientNorm "+ 
					StaticTools.prettyPrint(gradientNorms.get(0), "0.00000E00", 10)+ "\tvalue "+ StaticTools.prettyPrint(value.get(0), "0.000000E00",11)+"\n");
			}
			for(int i = 1; i < iterations.size(); i++){
			res.append("\tIteration:\t"+iterations.get(i)+"\tstep:"+StaticTools.prettyPrint(steps.get(i), "0.00E00", 6)+ "\tgradientNorm "+ 
					StaticTools.prettyPrint(gradientNorms.get(i), "0.00000E00", 10)+ 
					"\tvalue:\t"+ StaticTools.prettyPrint(value.get(i), "0.000000E00",11)+
					"\tvalueDiff:\t"+ StaticTools.prettyPrint((value.get(i-1)-value.get(i)), "0.000000E00",11)+
					"\n");
			}
		}
		return res.toString();
	}
	
	
	public void collectInitStats(Optimizer optimizer, Objective objective){
		startTime();
		iterations.add(-1);
		gradientNorms.add(MathUtils.L2Norm(objective.getGradient()));
		steps.add(0.0);
		value.add(objective.getValue());
	}
	
	public void collectIterationStats(Optimizer optimizer, Objective objective){
		iterations.add(optimizer.getCurrentIteration());
		gradientNorms.add(MathUtils.L2Norm(objective.getGradient()));
		steps.add(optimizer.getCurrentStep());
		value.add(optimizer.getCurrentValue());
	}
	
	
	public void collectFinalStats(Optimizer optimizer, Objective objective){
		stopTime();
		objectiveFinalStats = objective.finalInfoString();
	}
	
}
