package optimization.stopCriteria;

import java.util.ArrayList;

import optimization.gradientBasedMethods.Objective;

public class CompositeStopingCriteria implements StopingCriteria {
	
	ArrayList<StopingCriteria> criterias;
	
	public CompositeStopingCriteria() {
		criterias = new ArrayList<StopingCriteria>();
	}
	
	public void add(StopingCriteria criteria){
		criterias.add(criteria);
	}
	
	public boolean stopOptimization(Objective obj){
		for(StopingCriteria criteria: criterias){
			if(criteria.stopOptimization(obj)){
				return true;
			}
		}
		return false;
	}
	
	public void reset(){
		for(StopingCriteria criteria: criterias){
			criteria.reset();
		}
	}
}
