package optimization.stopCriteria;

import optimization.gradientBasedMethods.Objective;
import optimization.util.MathUtils;

public class ValueDifference implements StopingCriteria{
	
	/**
	 * Stop if the different between values is smaller than a treshold
	 */
	protected double valueConvergenceValue=0.01;
	protected double previousValue = Double.NaN;
	protected double currentValue = Double.NaN;
	
	public ValueDifference(double valueConvergenceValue){
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
			if(previousValue - currentValue   < valueConvergenceValue){
//				System.out.println("Leaving different in values is to small: Prev " 
//						+ previousValue + " Curr: " + currentValue 
//						+ " diff: " + (previousValue - currentValue));
				return true;
			}
			return false;
		}
		
	}
}
