package optimization.gradientBasedMethods.stats;

import java.util.ArrayList;

import optimization.gradientBasedMethods.Objective;
import optimization.gradientBasedMethods.Optimizer;
import optimization.gradientBasedMethods.ProjectedObjective;
import optimization.gradientBasedMethods.ProjectedOptimizer;
import optimization.util.MathUtils;
import optimization.util.StaticTools;


public class ProjectedOptimizerStats extends OptimizerStats{
	
	
	
	public void reset(){
		super.reset();
		projectedGradientNorms.clear();
	}
	
	ArrayList<Double> projectedGradientNorms = new ArrayList<Double>();

	public String prettyPrint(int level){
		StringBuffer res = new StringBuffer();
		res.append("Total time " + totalTime/1000 + " seconds \n" + "Iterations " + iterations.size() + "\n");
		res.append(objectiveFinalStats+"\n");
		if(level > 0){
			if(iterations.size() > 0){
			res.append("\tIteration"+iterations.get(0)+"\tstep: "+
					StaticTools.prettyPrint(steps.get(0), "0.00E00", 6)+ "\tgradientNorm "+ 
					StaticTools.prettyPrint(gradientNorms.get(0), "0.00000E00", 10)
					+ "\tdirection"+
					StaticTools.prettyPrint(projectedGradientNorms.get(0), "0.00000E00", 10)+
					"\tvalue "+ StaticTools.prettyPrint(value.get(0), "0.000000E00",11)+"\n");
			}
			for(int i = 1; i < iterations.size(); i++){
			res.append("\tIteration"+iterations.get(i)+"\tstep: "+StaticTools.prettyPrint(steps.get(i), "0.00E00", 6)+ "\tgradientNorm "+ 
					StaticTools.prettyPrint(gradientNorms.get(i), "0.00000E00", 10)+ 
					"\t direction "+
					StaticTools.prettyPrint(projectedGradientNorms.get(i), "0.00000E00", 10)+
					"\tvalue "+ StaticTools.prettyPrint(value.get(i), "0.000000E00",11)+
					"\tvalueDiff "+ StaticTools.prettyPrint((value.get(i-1)-value.get(i)), "0.000000E00",11)+
					"\n");
			}
		}
		return res.toString();
	}
	
	
	public void collectInitStats(Optimizer optimizer, Objective objective){
		startTime();
	}
	
	public void collectIterationStats(Optimizer optimizer, Objective objective){		
		iterations.add(optimizer.getCurrentIteration());
		gradientNorms.add(MathUtils.L2Norm(objective.getGradient()));
		projectedGradientNorms.add(MathUtils.L2Norm(optimizer.getDirection()));
		steps.add(optimizer.getCurrentStep());
		value.add(optimizer.getCurrentValue());
	}
	
	
	
	public void collectFinalStats(Optimizer optimizer, Objective objective){
		stopTime();
		objectiveFinalStats = objective.finalInfoString();
	}
	
}
