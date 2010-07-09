package optimization.stopCriteria;

import optimization.gradientBasedMethods.Objective;
import optimization.util.MathUtils;

public class NormalizedValueDifference implements StopingCriteria{
	
	/**
	 * Stop if the different between values is smaller than a treshold
	 */
	protected double valueConvergenceValue=0.01;
	protected double previousValue = Double.NaN;
	protected double currentValue = Double.NaN;
	
	public NormalizedValueDifference(double valueConvergenceValue){
		this.valueConvergenceValue = valueConvergenceValue;
	}
	
	public void reset(){
		previousValue = Double.NaN;
		currentValue = Double.NaN;
	}

	
	public boolean stopOptimization(Objective obj){
		if(Double.isNaN(currentValue)){
			currentValue = obj.getValue();
			return false;
		}else {
			previousValue = currentValue;
			currentValue = obj.getValue();
			if(previousValue != 0){
				double valueDiff = Math.abs(previousValue - currentValue)/Math.abs(previousValue);
				if( valueDiff  < valueConvergenceValue){
					System.out.println("Leaving different in values is to small: Prev " 
							+ (previousValue/previousValue) + " Curr: " + (currentValue/previousValue) 
							+ " diff: " + valueDiff);
					return true;
				}
			}else{
				double valueDiff = Math.abs(previousValue - currentValue);
				if( valueDiff  < valueConvergenceValue){
					System.out.println("Leaving different in values is to small: Prev " 
							+ (previousValue) + " Curr: " + (currentValue) 
							+ " diff: " + valueDiff);
					return true;
				}
			}

			return false;
		}
		
	}
}
